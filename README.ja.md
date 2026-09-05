# M2Encoder: M2 絶対角センサをホスト側から I2C で読むライブラリ

M2 絶対角センサを I2C で読むための Arduino 互換ライブラリです。
「M2」はこのエンコーダ系列の呼称です。センサは回転体そのものに取り付けたコード板から絶対角を求めるので、原点は機械の側に固有で、モータ、カップリング、減速機を交換してもリセットされません。

このリポジトリはホスト側だけです。センサのレジスタを読み、角度、有効かどうか、縮退の状態、次にすべき動作を返します。

センサ本体は含みません。コード板のパターン、復号処理、センサのファームウェアはここには公開していません。

## 配線

| 項目 | 値 |
|---|---|
| 役割 | センサが I2C の**スレーブ**、ホストがマスター |
| アドレス | **0x36**（7 ビット） |
| 速度 | 100 kHz（標準モード） |
| 電源 | 3.3 V（光学式）。プルアップはセンサ基板に実装済みなので追加しないでください |
| クロックストレッチ | なし。読み出しは常に即答します |

## 読み方

レジスタアドレスを書き、リピーテッドスタートの後に読みます。アドレスは自動で進むので、ライブラリは `0x00..0x16` を一度に読みます。

```cpp
m2enc::M2Encoder enc;
enc.begin(Wire, 0x36);
m2enc::Reading r;
if (enc.read(r) && r.valid) use(r.deg);   // 0.0 .. 359.8、0.2° 刻み
```

`r.valid` が真になるのは、状態の ABSOLUTE が立ち、PROBATION が立っていないときだけです。
セル値 `0xFFFF` は「まだ確定していない」を表します。数値ではないので、平均に混ぜないでください。

## 使用例

次のスケッチは `examples/basic_read/basic_read.ino` です。50 ms ごとにセンサを読み、推奨動作で分岐します。

```cpp
// basic_read: read the M2 absolute angle sensor and act on its status.
#include <Wire.h>
#include <M2Encoder.h>

m2enc::M2Encoder enc;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  if (!enc.begin(Wire, 0x36)) {
    Serial.println("sensor not found at 0x36");
  }
  enc.setSecondsPerCell(0.033f);   // your rotation rate; only used by report()
}

void loop() {
  m2enc::Reading r;
  if (!enc.read(r)) {
    Serial.println("bus error");
    delay(100);
    return;
  }

  switch (r.action) {
    case m2enc::NextAction::Use:
      Serial.print("angle "); Serial.print(r.deg, 1); Serial.println(" deg");
      break;
    case m2enc::NextAction::Rotate:
      // Angle is 0xFFFF until the sensor has seen a little motion (worst case 0.8 deg).
      Serial.print("not absolute yet, candidates "); Serial.println(r.n_cand);
      break;
    case m2enc::NextAction::Service:
      // Angle is still valid; one or more contacts are excluded. Plan maintenance.
      Serial.print("angle "); Serial.print(r.deg, 1);
      Serial.print(" deg (degraded, dead mask 0x"); Serial.print(r.dead, HEX); Serial.println(")");
      break;
    case m2enc::NextAction::CheckConfig:
      Serial.println("mark-width setting invalid; check installation");
      break;
  }
  delay(50);   // 10-100 ms polling is enough; the sensor updates internally at 1 ms
}
```

`r.action` はホスト側で状態バイトから決めています。角度が有効で除外も無い状態なら `Use`、まだ回転が要る間は `Rotate`、角度は有効でも接点が除外されていれば `Service`、設定エラーなら `CheckConfig` です。

## レジスタ表（ファームウェア 0x07）

| アドレス | バイト | 内容 |
|---|---|---|
| 0x00–0x01 | 2 | 角度。セル 0〜1799（×0.2 で度）、リトルエンディアン。0xFFFF は未確定 |
| 0x02–0x03 | 2 | 生の 15 ビット読み取り値（デバッグ用） |
| 0x04 | 1 | 状態フラグ（下表） |
| 0x05 | 1 | 生存モジュールのマスク（bit 0〜2、リング版） |
| 0x06 | 1 | マーク幅の設定値 w |
| 0x07 | 1 | ファームウェア版数（0x07） |
| 0x08–0x0D | 6 | デバッグ用: 生入力、デバウンス後入力、リセット回数、I2C エラー回数、役割、モジュール番号 |
| 0x0E–0x0F | 2 | 位置候補数。1 なら一意 |
| 0x10–0x11 | 2 | 確定した固着接点のビットマップ |
| 0x12 | 1 | 復号の矛盾検出回数 |
| 0x13 | 1 | 自動修復の採用回数 |
| 0x14–0x15 | 2 | 暫定除外（経過観察中）のビットマップ |
| 0x16 | 1 | 直前の事象の回転方向: 0 不明 / 1 正転 / 2 逆転 / 3 混在 |
| 上記以外 | 1 | 0xEE |

### 状態フラグ（0x04）

| ビット | 名前 | 意味 | ホスト側の推奨動作 |
|---|---|---|---|
| 0x01 | ABSOLUTE | 角度が一意に決まっている | 角度を使う |
| 0x02 | DEGRADED | 接点またはモジュールを除外して動作中 | 使用継続。保守を予約 |
| 0x04 | NEED_MOTION | 証拠不足。角度は 0xFFFF | 少し回して読み直す |
| 0x08 | CFG_ERROR | マーク幅の設定が不正 | 据付設定を確認 |
| 0x10 | BIT_FAULT | 固着接点を特定して除外済み | 交換を予約 |
| 0x20 | PROBATION | 暫定除外中。角度はリリース相当ではない | 角度を使わない |
| 0x40 | REVERSED | 直前の事象が逆回転 | 参考情報 |

## 使う側で見込んでおくこと

- 電源投入直後は必ず 0xFFFF / NEED_MOTION から始まります。キャリブレーションは不要ですが、わずかな回転が要ります。健全時の最悪値は 4 セル（0.8°）です。
- 読み出し周期は 10〜100 ms で十分です。センサ側は 1 ms 周期で内部更新しているので、それより速く読んでも値は増えません。
- 0x00–0x01 は 2 バイトを一度に読み、同じ時点の値にしてください。
- 現行の読み取り方式の回転速度の上限は毎分約 2 回転です。

## ライセンス

MIT。ライセンスの対象はこのホスト側コードだけです。このリポジトリの公開は、いかなる特許の実施許諾も与えず、含意せず、放棄しません。センサ本体、その符号化方式、ファームウェアについての権利は移転しません。
