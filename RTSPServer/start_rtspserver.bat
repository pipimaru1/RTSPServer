REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■
REM 5004 受信ポート
REM 8554 発信ポート
REM default パス(任意)
REM ポートを作成する数
REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■

RTSPServer.exe 5004 8554 default 8

REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■
REM テスト用ffmpeg例 USBカメラの映像をRTSPserverに送信する例
REM 標準画質
REM ffmpeg -f dshow -i video="Logicool BRIO"  -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency  -f rtp rtp://127.0.0.1:5004
REM HD画質
REM ffmpeg -f dshow -framerate 30 -video_size 1920x1080 -i video="Logicool BRIO" -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency -f rtp rtp://127.0.0.1:5004
REM ffmpeg -f dshow -framerate 30 -video_size 1920x1080 -i video="Logicool BRIO" -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency -f rtp rtp://127.0.0.1:5005
REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■


REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■
REM テスト用FFplay例 RTSPServerの映像をffplayで受信する例
REM ffplay rtsp://127.0.0.1:8554/test
REM ffplay rtsp://127.0.0.1:8555/sisi
REM ■■■■■■■■■■■■■■■■■■■■■■■■■■■

