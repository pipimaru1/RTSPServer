REM ������������������������������������������������������
REM 5004 ��M�|�[�g
REM 8554 ���M�|�[�g
REM default �p�X(�C��)
REM �|�[�g���쐬���鐔
REM ������������������������������������������������������

RTSPServer.exe 5004 8554 default 8

REM ������������������������������������������������������
REM �e�X�g�pffmpeg�� USB�J�����̉f����RTSPserver�ɑ��M�����
REM �W���掿
REM ffmpeg -f dshow -i video="Logicool BRIO"  -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency  -f rtp rtp://127.0.0.1:5004
REM HD�掿
REM ffmpeg -f dshow -framerate 30 -video_size 1920x1080 -i video="Logicool BRIO" -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency -f rtp rtp://127.0.0.1:5004
REM ffmpeg -f dshow -framerate 30 -video_size 1920x1080 -i video="Logicool BRIO" -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency -f rtp rtp://127.0.0.1:5005
REM ������������������������������������������������������


REM ������������������������������������������������������
REM �e�X�g�pFFplay�� RTSPServer�̉f����ffplay�Ŏ�M�����
REM ffplay rtsp://127.0.0.1:8554/test
REM ffplay rtsp://127.0.0.1:8555/sisi
REM ������������������������������������������������������

