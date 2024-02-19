# RZ/T2M向け高機能エンコーダインタフェースライブラリ及びサンプルプログラム

<img src="https://github.com/shimafuji-fvio/encif_rzt2_t-format-like_quad/assets/48343451/916f39cb-4b3f-4c17-80d6-181eddaf1b56" width="30%" align="right" />

本エンコーダインタフェースライブラリは多摩川精機のT-format互換の高機能通信ライブラリです。

RZ/T2Mでは最大8個のエンコーダと同時に通信可能です。

# 特徴

特徴は下記の通り。
- ENCIF 1チャンネルで最大4個のエンコーダと同時に通信可能
  - 送信データは全エンコーダ共通
  - 送信先のEnable信号を個別に制御
  - 受信データは個別に受信
  - 受信データは1フィールド(10bit)ごとにstartビットを検出
  - 受信可能なビットレート偏差は±3%
  - 各エンコーダ間で受信データタイミングが遅延しても受信可能
  - 送信は1..4フィールド、受信は1..15フィールドが指定可能
- 送信開始トリガはレジスタ書き込みとELCから選択可能
- 任意の生成多項式(8bit)でCRCチェック可能(最終受信フィールドのデータをCRCデータとする)
- 送受信ビットレートは、2.5/4/5/8/10MHzから選択可能
- 受信終了時のCPU割り込みはエッジ/レベルから選択可能

なお、下記の通り使用制限があります。
- 送信データは全てユーザ側で設定すること
- 受信データはENCIFのFIFO経由のみとなる
  - 受信データチェックはCRCのみ
- 通信回数の制限あり
  - 2^24=16,777,216回まで(キャリア周波数10KHzの場合は約27分)
  - 別途ライセンス契約により通信回数の制限は解除可能

# 要件

- ルネサス社製開発ツール
  - [e2studio and RZ/T2 FSP installer (FSP ver 1.3.0)](https://www.renesas.com/jp/ja/document/sws/e-studio-and-rzt2-fsp-installer)
- ルネサス社製サンプルコード
  - [RZ/T2M FA-CODER Sample Program (ver 1.60)](https://www.renesas.com/jp/ja/document/scd/rzt2m-group-encoder-if-fa-coder-sample-program)
  - [RZ/T2M Encoder I/F Configuration Library (ver 1.60)](https://www.renesas.com/jp/ja/document/scd/rzt2m-group-encoder-if-configuration-library)
- ルネサス製評価ボード
  - [Renesas Starter Kit+ for RZ/T2M](https://www.renesas.com/jp/ja/products/microcontrollers-microprocessors/rz-mpus/rzt2m-rsk-renesas-starter-kit-rzt2m)


# インストール手順

- e2studio(with FSP ver 1.3.0)のインストール
- RZ/T2M FA-CODER sample Program(r11an0587xx0160-rzt2m-fa-coder.zip)に含まれるgcc版のRZ_T2_fac.zipを解凍
- RZ/T2M Encoder I/F Configuration Library(r01an6365xx0160-rzt2m-encoder.zip)に含まれるlibディレクトリを上記解凍先にコピー
- 本エンコーダライブラリを上記解凍先に上書きコピー

# 使用方法

- ルネサス製評価ボードとエンコーダを接続する
  - CN1に下記の端子アサインで接続する

![rsk-encoder_pin](https://github.com/shimafuji-fvio/encif_rzt2_t-format-like_quad/assets/48343451/b63cee45-218c-412d-8aaf-80ab73303e2d)

- ルネサス製評価ボードとPCを接続する
  - ルネサス製評価ボードのコンソール用USBコネクタ(CN16)とPCを接続する
  - ルネサス製評価ボードのデバッガ用USBコネクタ(J10)とPCを接続する

- e2studioの立ち上げとコンパイル、実行
  - インストールしたプロジェクト一式をe2studioにインポートする
  - プロジェクトをビルドする
  - ルネサス製評価ボードの電源をONにする
  - デバッグを実行する

# サンプルプログラムの概要

- 接続するエンコーダの設定
  - ビットレート2.5Mbps
  - CRCをx^8+1に設定
  - CRCチェック=on, CPU割り込み=on, 割り込みモード=レベル
  - 3個のエンコーダと同時通信

- マニュアルトリガによる通信
  - id=0,1,2,3,Dの送信データをそれぞれレジスタ書き込みで実行
  - 受信結果はENCIFの割り込み処理、受信データはDMAで引き取り、受信結果と受信データを表示

- ELCトリガによる通信
  - GPTタイマを用いて100[ms]間隔でELC経由でENCIFにトリガイベントを発生させる
  - 同ELCイベントでid=0の送信データで通信開始
  - 受信結果はENCIFの割り込み処理、受信データはDMAで引き取り、受信結果と受信データを表示
  - 上記を繰り返し実行

- タイミングチャート

<img src="https://github.com/shimafuji-fvio/encif_rzt2_t-format-like_quad/assets/48343451/efbd4139-a3a9-40c7-9c47-974dd628afe7" width="90%" />

T0：	SRQ出力前のDE時間

T1：	SRQ出力後のDE時間

T2：	RX0受信前の受信無効時間

TOT0：	RX0受信までのタイムアウト時間

TOT1：	RXnからRXn+1までのタイムアウト時間

※ 単位は通信ビットレート周期の1/5

<img src="https://github.com/shimafuji-fvio/encif_rzt2_t-format-like_quad/assets/48343451/59b9d470-b6da-4ece-9d05-a67a39f16796" width="50%" />


# 著者

- [シマフジ電機株式会社](http://www.shimafuji.co.jp/)
- E-mail: shimafuji-fvio@shimafuji.co.jp

# ライセンス

本ライブラリとサンプルプログラムは[MITライセンス](https://en.wikipedia.org/wiki/MIT_License)です。

