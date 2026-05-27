This is a RTSP Broadcast center.
<img width="1046" height="462" alt="image" src="https://github.com/user-attachments/assets/08f23647-def8-4b81-a9b4-43326c2a4195" />
# RTSPServerGUI 説明書

## 概要

`RTSPServerGUI` は、GStreamer を使用して RTP/UDP で送られてきた H.264 映像を受信し、チャンネルごとに RTSP 配信する Windows GUI アプリケーションです。HLSでHTTP配信することもできます。

最大 32 チャンネルを同時に扱うことを想定しており、各チャンネルに対して次の設定を持ちます。

- 入力ポート: 送信元から RTP/UDP 映像を受信するポート
- 出力ポート: RTSP サーバーとして待ち受けるポート
- チャンネル名: RTSP の URL パスに使用する名前
- HTTP/HLS 用設定: HLS の URL 表示に使用する HTTP ポートなど

通常運用では、送信側から以下のような H.264 RTP ストリームを `Input Port` 宛に送信します。

```bash
ffmpeg -re -f lavfi -i testsrc2=size=1280x720:rate=30 \
  -an -c:v libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency \
  -f rtp rtp://<RTSPServerGUI実行PCのIPアドレス>:5004
```

`RTSPServerGUI` 側で `START` を押すと、受信した映像を次のような RTSP URL で配信します。

```text
rtsp://<RTSPServerGUI実行PCのIPアドレス>:8554/default
```

## 動作環境

### 実行環境

- Windows
- GStreamer 1.0 Runtime
- RTSPServerGUI.exe

GStreamer がインストールされていない場合、アプリケーション起動時または配信開始時に GStreamer 関連 DLL が見つからず失敗することがあります。

### 開発・ビルド環境

ソースからビルドする場合は、以下が必要です。

- Visual Studio 2022
- Windows SDK 10
- C++17
- GStreamer 1.0 Runtime
- GStreamer 1.0 Development SDK

プロジェクトは `RTSPServer.sln` に含まれています。GUI 版は `RTSPServerGUI` プロジェクトです。

`RTSPServerGUI.vcxproj` では x64 構成で以下の環境変数を参照しています。

```text
GSTREAMER_1_0_ROOT_X86_64
```

この環境変数には GStreamer のインストール先を設定してください。例:

```text
C:\gstreamer\1.0\msvc_x86_64
```

また、実行時に GStreamer の `bin` フォルダが `PATH` に含まれている必要があります。

```text
%GSTREAMER_1_0_ROOT_X86_64%\bin
```

## 起動方法

### 1. GStreamer をインストールする

実行する PC に GStreamer 1.0 Runtime をインストールします。

開発・ビルドも行う場合は、Runtime に加えて Development SDK もインストールしてください。

### 2. RTSPServerGUI をビルドする

Visual Studio で `RTSPServer.sln` を開きます。

推奨構成:

```text
構成: Release
プラットフォーム: x64
プロジェクト: RTSPServerGUI
```

ビルド後、`RTSPServerGUI.exe` を実行します。

### 3. 設定ファイルを配置する

設定ファイルは、`RTSPServerGUI.exe` と同じフォルダに配置します。

`settings.ini` が存在しない場合は、アプリケーション内のデフォルト値で起動します。アプリケーション終了時に設定が保存されます。

### 4. アプリケーションを起動する

`RTSPServerGUI.exe` をダブルクリックして起動します。

初回起動時は、各チャンネルに次のデフォルト値が入ります。

| チャンネル | Input Port | Output Port | Channel Name |
|---:|---:|---:|---|
| CH01 | 5004 | 8554 | default |
| CH02 | 5006 | 8556 | default |
| CH03 | 5008 | 8558 | default |
| CH04 | 5010 | 8560 | default |
| CH05 | 5012 | 8562 | default |
| CH06 | 5014 | 8564 | default |
| CH07 | 5016 | 8566 | default |
| CH08 | 5018 | 8568 | default |
| CH09 | 5020 | 8570 | default |
| CH10 | 5022 | 8572 | default |
| CH11 | 5024 | 8574 | default |
| CH12 | 5026 | 8576 | default |
| CH13 | 5028 | 8578 | default |
| CH14 | 5030 | 8580 | default |
| CH15 | 5032 | 8582 | default |
| CH16 | 5034 | 8584 | default |
| CH17 | 5036 | 8586 | default |
| CH18 | 5038 | 8588 | default |
| CH19 | 5040 | 8590 | default |
| CH20 | 5042 | 8592 | default |
| CH21 | 5044 | 8594 | default |
| CH22 | 5046 | 8596 | default |
| CH23 | 5048 | 8598 | default |
| CH24 | 5050 | 8600 | default |
| CH25 | 5052 | 8602 | default |
| CH26 | 5054 | 8604 | default |
| CH27 | 5056 | 8606 | default |
| CH28 | 5058 | 8608 | default |
| CH29 | 5060 | 8610 | default |
| CH30 | 5062 | 8612 | default |
| CH31 | 5064 | 8614 | default |
| CH32 | 5066 | 8616 | default |

## 設定ファイル

### ファイル名

現在の実装では以下のファイルを使用します。

```text
settings.ini
```

### 配置場所

`RTSPServerGUI.exe` と同じフォルダに配置します。

```text
RTSPServerGUI.exe
settings.ini
```

### 設定ファイルの形式

チャンネルごとに `[CH01]` から `[CH32]` のセクションを使用します。

各チャンネルの設定項目は以下です。

| キー | 内容 | 例 |
|---|---|---|
| `InPort` | RTP/UDP 映像を受信する入力ポート | `5004` |
| `OutPort` | RTSP サーバーの待ち受けポート | `8554` |
| `Channel` | RTSP URL のパス名 | `default` |

HTTP/HLS 関連の設定は `[HTTP]` セクションに保存されます。

| キー | 内容 | 初期値 | 備考 |
|---|---|---:|---|
| `Port` | HLS URL 表示に使用する HTTP ポート | `8080` | URL コンボボックス生成に使用 |
| `HLS` | HLS 用文字列 | `hls` | 現在の URL 生成処理では実質未使用 |

### 設定例

```ini
[HTTP]
Port=8080
HLS=hls

[CH01]
InPort=5004
OutPort=8554
Channel=default

[CH02]
InPort=5006
OutPort=8556
Channel=camera02

[CH03]
InPort=5008
OutPort=8558
Channel=camera03
```

設定されていないチャンネルは、アプリケーション内のデフォルト値で起動します。

### URL の生成例

`CH01` が次の設定の場合:

```ini
[CH01]
InPort=5004
OutPort=8554
Channel=default
```

RTSP URL は次の形式になります。

```text
rtsp://127.0.0.1:8554/default
rtsp://<ローカルIPアドレス>:8554/default
```

HLS URL は次の形式で表示されます。

```text
http://127.0.0.1:8080/default-8554/index.m3u8
http://<ローカルIPアドレス>:8080/default-8554/index.m3u8
```

HLS の実ファイルは、GStreamer の `hlssink` により次のフォルダへ出力されます。

```text
C:\hls\<Channel>-<OutPort>\index.m3u8
C:\hls\<Channel>-<OutPort>\seg00000.ts
C:\hls\<Channel>-<OutPort>\seg00001.ts
...
```

例:

```text
C:\hls\default-8554\index.m3u8
```

> 注意: ソースコード内には HTTP サーバーを起動する処理は確認できません。  
> HLS URL で視聴する場合は、別途 HTTP サーバーを用意し、`C:\hls` 配下を公開してください。

## 画面構成

<img width="1046" height="462" alt="image" src="https://github.com/user-attachments/assets/08f23647-def8-4b81-a9b4-43326c2a4195" />

メイン画面には、CH01 から CH32 までの行があります。

主な項目は以下です。

| 項目 | 説明 |
|---|---|
| `START` | 対象チャンネルの RTSP 中継を開始します。 |
| `TEST` | 入力映像を使わず、GStreamer のテストパターンを配信します。 |
| `STOP` | 対象チャンネルの配信を停止します。 |
| `Input Port` | RTP/UDP 映像を受信するポートです。 |
| `Output Port` | RTSP サーバーとして公開するポートです。 |
| `Channel Name` | RTSP URL の末尾に使うチャンネル名です。 |
| `ON AIR` | 映像受信中の状態を表示します。赤色が受信中、灰色が停止または未受信です。 |
| `Source Address` | 映像送信元の IP アドレスとポートを表示します。 |
| `Bitrate[Kbps]` | 受信中の映像ビットレートを kbps で表示します。 |
| `RTSP URL` | 視聴用 RTSP URL の候補を表示します。選択するとクリップボードにコピーされます。 |
| `HTTP URL` | HLS 視聴用 URL の候補を表示します。選択するとクリップボードにコピーされます。 |
| `CATCH` | 受信状態表示用のチェックボックスです。ユーザー操作は無効化されています。 |

## 操作方法

### チャンネルを開始する

1. 対象チャンネルの `Input Port` を入力します。
2. 対象チャンネルの `Output Port` を入力します。
3. 対象チャンネルの `Channel Name` を入力します。
4. `START` を押します。

開始すると、そのチャンネルの RTSP サーバーが起動します。

例:

| 設定項目 | 値 |
|---|---:|
| Input Port | 5004 |
| Output Port | 8554 |
| Channel Name | default |

この場合、送信側は次の宛先に RTP/UDP で映像を送ります。

```text
rtp://<RTSPServerGUI実行PCのIPアドレス>:5004
```

視聴側は次の URL で RTSP 映像を受信します。

```text
rtsp://<RTSPServerGUI実行PCのIPアドレス>:8554/default
```

### チャンネルを停止する

対象チャンネルの `STOP` を押します。

停止すると、RTSP クライアント接続を閉じ、GStreamer のメインループを終了します。停止後は `Input Port`、`Output Port`、`Channel Name` を再編集できます。

### テストパターンを配信する

対象チャンネルの `TEST` を押します。

`TEST` では、外部から映像を受信せず、GStreamer の `videotestsrc` による SMPTE テストパターンを配信します。

配信経路や RTSP URL の確認、視聴側の接続確認に使用できます。

### 全チャンネルを開始する

画面上部の `ALL` 行にある `START` を押します。

CH01 から CH32 まで、全チャンネルの開始処理を順番に実行します。

### 全チャンネルをテスト開始する

画面上部の `ALL` 行にある `TEST` を押します。

CH01 から CH32 まで、全チャンネルをテストパターン配信で開始します。

### 全チャンネルを停止する

画面上部の `ALL` 行にある `STOP` を押します。

CH01 から CH32 まで、全チャンネルの停止処理を順番に実行します。

### RTSP URL をコピーする

`RTSP URL` のコンボボックスには、次の URL が表示されます。

- `127.0.0.1` を使用したローカル確認用 URL
- 実行 PC のローカル IP アドレスを使用した外部アクセス用 URL

コンボボックスから URL を選択すると、その URL がクリップボードにコピーされます。

### HLS URL をコピーする

`HTTP URL` のコンボボックスには、HLS 用 URL が表示されます。

コンボボックスから URL を選択すると、その URL がクリップボードにコピーされます。

ただし、現在のソースコードでは HTTP サーバー自体は起動していません。HLS を使用する場合は、`C:\hls` を HTTP で公開する別プロセスが必要です。

## 受信状態の表示

映像を受信すると、対象チャンネルの表示が更新されます。

- `CATCH`: 受信中にチェック状態になります。
- `ON AIR`: 受信中は赤色になります。
- `Source Address`: 送信元 IP アドレスとポートを表示します。
- `Bitrate[Kbps]`: 現在の受信ビットレートを表示します。

内部的には、`udpsrc` に対して受信監視を行い、一定時間受信がない場合は無信号として扱います。

## 送信側の例

### テストパターンを RTP 送信する例

```bash
ffmpeg -re -f lavfi -i testsrc2=size=1920x1080:rate=30 \
  -an -c:v libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency \
  -f rtp rtp://127.0.0.1:5004
```

### USB カメラを RTP 送信する例

```bash
ffmpeg -f dshow -framerate 30 -video_size 1920x1080 -i video="FHD Camera" \
  -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency \
  -f rtp rtp://127.0.0.1:5004
```

別 PC から送る場合は、`127.0.0.1` を RTSPServerGUI 実行 PC の IP アドレスに置き換えてください。

```bash
ffmpeg -re -f lavfi -i testsrc2=size=1280x720:rate=30 \
  -an -c:v libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency \
  -f rtp rtp://192.168.1.10:5004
```

## 視聴側の例

### ffplay で視聴する

```bash
ffplay rtsp://127.0.0.1:8554/default
```

別 PC から視聴する場合:

```bash
ffplay rtsp://192.168.1.10:8554/default
```

## 注意事項

### ポート番号について

`Input Port` と `Output Port` は、1 から 65535 の範囲で指定してください。

複数チャンネルを使用する場合は、ポート番号が重複しないようにしてください。初期値では、入力ポート・出力ポートともに 2 ずつずらした値が設定されています。

### ファイアウォールについて

別 PC から映像を送信または視聴する場合は、Windows ファイアウォールで以下の通信を許可してください。

- RTP/UDP 入力ポート
- RTSP 出力ポート
- HLS を使用する場合の HTTP ポート

### Channel Name について

`Channel Name` は RTSP URL のパスになります。

例:

```text
Channel Name = camera01
RTSP URL     = rtsp://<IPアドレス>:8554/camera01
```

空欄の場合は開始時に入力エラーになります。

また、ソースコード内のコメントでは「一つの出力ポートに複数チャンネルはサポートしていない」とされています。チャンネルごとに異なる `Output Port` を設定してください。

### HLS について

RTSPServerGUI は GStreamer の `hlssink` により、次の形式で HLS ファイルを作成します。

```text
C:\hls\<Channel>-<OutPort>\index.m3u8
C:\hls\<Channel>-<OutPort>\segXXXXX.ts
```

HLS セグメント設定は、ソースコード上では以下のようになっています。

```text
target-duration = 2
playlist-length = 20
max-files       = 40
```

HLS の URL 表示はありますが、HTTP サーバー機能はこのソース内では確認できません。HLS をブラウザやプレイヤーから視聴する場合は、`C:\hls` を公開する HTTP サーバーを別途起動してください。

## 終了方法

画面右下の `OK`、`キャンセル`、またはウィンドウの閉じるボタンで終了します。

終了時には、起動中のチャンネルを停止し、設定を `settings.ini` に保存します。

## トラブルシュート

### アプリが起動しない

GStreamer の DLL が見つからない可能性があります。

`PATH` に次のフォルダが含まれているか確認してください。

```text
%GSTREAMER_1_0_ROOT_X86_64%\bin
```

### START を押しても映像が出ない

以下を確認してください。

- 送信側が H.264 RTP を送っているか
- 送信先ポートが `Input Port` と一致しているか
- Windows ファイアウォールで UDP 入力が許可されているか
- 視聴側 URL のポートが `Output Port` と一致しているか
- `Channel Name` が URL のパスと一致しているか

### ON AIR が赤にならない

映像が `Input Port` に届いていない可能性があります。

`Source Address` に送信元 IP アドレスが表示されるか確認してください。表示されない場合は、送信側の宛先 IP アドレス、ポート番号、ファイアウォールを確認してください。

### HLS URL にアクセスできない

`C:\hls` に `index.m3u8` と `.ts` ファイルが作成されているか確認してください。

ファイルが作成されているのに URL でアクセスできない場合は、HTTP サーバー側の公開フォルダ設定を確認してください。

