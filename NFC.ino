#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DFPlayer_Mini_Mp3.h>
#include <MFRC522.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <Wire.h>

#define DEDUCT_BTN 3
#define ADD_BTN 4
#define SUBMIT_BTN 5

int credit;
int cardPoint;
byte phase;
byte LED = 2;

// RC522
byte trailerBlock = 7;
byte buffer[18];
byte size = sizeof(buffer);
byte sector = 1;
byte blockAddr = 4;
MFRC522::StatusCode status;
constexpr uint8_t RST_PIN = 9;     // RSTピンの指定
constexpr uint8_t SS_PIN = 10;     // SSピンの指定
MFRC522 mfrc522(SS_PIN, RST_PIN);  // RC522と接続
MFRC522::MIFARE_Key key;           //認証キーの指定
MFRC522::PICC_Type piccType;

Adafruit_SSD1306 display(-1);     // ディスプレイ変数の宣言
SoftwareSerial mySerial(15, 16);  // DFplayer用のRX, TXピンの指定

void setup() {
  Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // ディスプレイを起動
  while (!Serial)
    ;
  SPI.begin();  // OLEDとSPI通信を開始

  Serial.println("セットアップ中");
  display.clearDisplay();       // 一度初期化
  display.setTextSize(1);       // 出力する文字の大きさ
  display.setTextColor(WHITE);  // 出力する文字の色
  display.setCursor(0, 0);      // カーソル位置の指定
  display.println("SET UP");    // ディスプレイへ表示する文字列

  delay(10);
  mfrc522.PCD_Init();
  mfrc522
      .PCD_DumpVersionToSerial();  // 0x12が出ればRC522はArduinoに認識されている
  for (byte i = 0; i < 6; i++) {  // key.keyByte[5] = {255, 255, 255, 255, 255};
    key.keyByte[i] = 0xFF;
  }

  mySerial.begin(9600);
  mp3_set_serial(mySerial);  // set softwareSerial for DFPlayer-mini mp3
                             // module
  mp3_stop();

  // 0 RX
  // 1 TX
  pinMode(2, OUTPUT);        // LED
  pinMode(3, INPUT_PULLUP);  //ポイント-
  pinMode(4, INPUT_PULLUP);  //ポイント+
  pinMode(5, INPUT_PULLUP);  //送信
  // 6
  // 7
  // 8
  // 9 RST_PIN
  // 10 SDA_PIN
  // 11 MOSI_PIN
  // 12 MISO_PIN
  // 13 SCK_PIN
  pinMode(A0, INPUT);  //アナログ、モード設定
                       // A1 Software RX DFplayer-TX
                       // A2 Software TX Dfplayer-RX
                       // A3
                       // A4
                       // A5
}

void loop() {
  mfrc522.PICC_HaltA();       // Halt PICC
  mfrc522.PCD_StopCrypto1();  // Stop encryption on PCD
  int mode = analogRead(A0);
  bool Deduct = digitalRead(DEDUCT_BTN);
  bool Add = digitalRead(ADD_BTN);
  bool submit = digitalRead(SUBMIT_BTN);

  //モードの判定文
  if (mode < 250)
    PDis("READER", mode);
  else if (450 < mode)
    PDis("RESET", mode);
  else if (250 <= mode && mode <= 450) {
    PDis("PUSH YELLO", mode);
    if (submit == 0) Select();
  }
  if (Deduct == 0 && Add == 0) {
    music();
  }

  // DFplayerでloop内では誘導音を流し続けるためのプログラム
  mp3_get_state();
  int state = mp3_wait_state();
  Serial.println(state);  //再生中は511,そうでないときは510となる
  if (state == 510) {
    Serial.println("play 0002_kaisatu2");
    mp3_set_volume(
        10);  // 0~30で設定。get_state()よりも後に書かなければならない
    mp3_play(2);
    delay(100);
  }

  //カードがかざされるとif文を突破
  if (!mfrc522.PICC_IsNewCardPresent())
    return;  // Mifareカードの確認（新しいカードが無ければ終了し、loop関数を繰り返す）
  if (!mfrc522.PICC_ReadCardSerial())
    return;  // Mifareカードのデータ読み込み（読み取れなければ終了し、loop関数を繰り返す）
  auth();    //認証

  //各モードの仕事をする
  if (450 < mode) {
    NFCWrite(5, 0, 0);  // RESET
  } else {  // READERの場合と、PUSH YELLOでSelect()に移行していない場合
    READER();
    delay(1500);
  }
}

void Select() {
  int credit = 0;
  int cardPoint = 0;
  int point = 0;
  phase = 1;  //初期化
  mp3_stop();
  mp3_set_volume(15);
  mp3_play(4);  //ボタンを押した時の音

  while (phase < 5) {
    Serial.println(phase);
    bool Deduct = digitalRead(DEDUCT_BTN);
    bool Add = digitalRead(ADD_BTN);
    bool submit = digitalRead(SUBMIT_BTN);
    //    bool old_submit = submit;
    if (submit == 0) {
      phase++;
      mp3_play(4);  //ボタンを押した時の音
      delay(150);
    }

    if (phase == 1) {  //クレジット操作
      PDis("credit", credit);
      if (Deduct == 0) {
        mp3_play(4);
        credit--;
        delay(100);
      }
      if (Add == 0) {
        mp3_play(4);
        credit++;
        delay(100);
      }
    }

    if (phase == 2) {  //ポイント操作
      cardPoint = point * 10;
      PDis("point", cardPoint);
      if (Deduct == 0) {
        mp3_play(4);
        point--;
        delay(100);
      }
      if (Add == 0) {
        mp3_play(4);
        point++;
        delay(100);
      }
    }

    if (phase == 3) {               //確認
      display.clearDisplay();       // 一度初期化
      display.setTextSize(1);       // 出力する文字の大きさ
      display.setTextColor(WHITE);  // 出力する文字の色
      display.setCursor(0, 0);      // カーソル位置の指定
      display.println("Are you sure?");  // ディスプレイへ表示する文字列
      display.print("credit =");   // ディスプレイへ表示する文字列
      display.println(credit);     // ディスプレイへ表示する文字列
      display.print("point =");    // ディスプレイへ表示する文字列
      display.println(cardPoint);  // ディスプレイへ表示する文字列
      display.println("BLUE = BACK select");  // ディスプレイへ表示する文字列
      display.display();  //ディスプレイ情報の更新

      if (Deduct == 0) BackSelect();
    }

    while (phase == 4) {
      display.clearDisplay();       // 一度初期化
      display.setTextSize(2);       // 出力する文字の大きさ
      display.setTextColor(WHITE);  // 出力する文字の色
      display.setCursor(0, 0);      // カーソル位置の指定
      display.println("TOUCH CARD");  // ディスプレイへ表示する文字列
      display.setTextSize(1);
      display.print("credit =");   // ディスプレイへ表示する文字列
      display.println(credit);     // ディスプレイへ表示する文字列
      display.print("point =");    // ディスプレイへ表示する文字列
      display.println(cardPoint);  // ディスプレイへ表示する文字列
      display.println("phase4");   // ディスプレイへ表示する文字列
      display.display();           //ディスプレイ情報の更新

      if (!mfrc522.PICC_IsNewCardPresent())
        continue;  // Mifareカードの確認（新しいカードが無ければ終了し、loop関数を繰り返す）
      if (!mfrc522.PICC_ReadCardSerial())
        continue;  // Mifareカードのデータ読み込み（読み取れなければ終了し、loop関数を繰り返す）
      auth();

      mfrc522.MIFARE_Read(blockAddr, buffer, &size);
      dump_byte_array(buffer, 16);
      Serial.println();
      credit += buffer[0];             //回数
      cardPoint += buffer[1];          //ポイント
      NFCWrite(credit, cardPoint, 1);  //ブース番号を選んで変更
      phase = 5;
    }
  }
}

void BackSelect() {
  bool Deduct = 1;
  bool submit = 1;

  while (submit == 1) {
    Deduct = digitalRead(DEDUCT_BTN);
    submit = digitalRead(SUBMIT_BTN);
    if (Deduct == 0) {
      phase++;
      mp3_play(4);
      delay(100);
    }

    if (phase == 1) {
      PDis("CREDIT", phase);
    } else if (phase == 2) {
      PDis("Point", phase);
    } else if (phase == 3) {
      PDis("Submit", phase);
    } else if (phase == 4) {
      phase = 1;
    }
    if (submit == 0) {
      mp3_play(4);
      break;
    }
  }
}

void auth() {
  mp3_stop();
  mp3_set_volume(25);  // 0~30で設定。get_state()よりも後に書かなければならない
  mp3_play(1);
  digitalWrite(LED, LOW);
  Serial.println("auth()");
  piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  if (piccType != MFRC522::PICC_TYPE_MIFARE_UL) {
    Serial.println("Authenticating using key A...");
    status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
        MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.println("auth() error");
      mp3_play(5);
      MDis("auth()", "status error", "Card is distant", "one more prease");
      delay(1500);
      return;
    }
  }
}

void READER() {
  digitalWrite(LED, LOW);

  mfrc522.MIFARE_Read(blockAddr, buffer, &size);
  dump_byte_array(buffer, 16);

  int credit = buffer[0];     //回数
  int cardPoint = buffer[1];  //ポイント

  Serial.println("現在の回数");
  Serial.println(credit);
  Serial.println("現在のポイント");
  Serial.println(cardPoint);

  display.clearDisplay();   //一度初期化
  display.setCursor(0, 0);  //カーソル位置の指定
  display.setTextSize(2);   // 出力する文字の大きさ
  display.println("READER");
  display.setTextSize(1);  // 出力する文字の大きさ
  display.print("credit= ");
  display.println(credit);
  display.print("cardPoint= ");
  display.println(cardPoint);
  display.display();  //ディスプレイ情報の更新
}

void NFCWrite(int CR, int CP, byte BN) {  //クレジット、ポイント、ブース番号
  digitalWrite(LED, LOW);                 // LED消灯　タッチ禁止
  piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  byte dataBlock[16] = {CR, CP, BN};
  if (piccType != MFRC522::PICC_TYPE_MIFARE_UL) {
    Serial.println("Authenticating again using key B");
    status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
        MFRC522::PICC_CMD_MF_AUTH_KEY_B, trailerBlock, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      digitalWrite(LED, LOW);
      mp3_play(5);
      MDis("WRITE(),PCD_Authenticate", "status error", "Card is distant",
           "one more prease");
      delay(1500);
      return;
    }
  }

  // Write data to the block
  Serial.print("Writing data into block ");
  Serial.print(blockAddr);
  dump_byte_array(dataBlock, 16);
  status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(blockAddr, dataBlock, 16);
  if (status != MFRC522::STATUS_OK) {
    digitalWrite(LED, LOW);
    mp3_play(5);
    MDis("WRITE(),MIFARE_Write", "status error", "Write failed",
         "one more prease");
    delay(1500);
    return;
  }

  Serial.println("再確認");
  READER();
  Serial.println("再確認s");
  mfrc522.PICC_HaltA();       // Halt PICC
  mfrc522.PCD_StopCrypto1();  // Stop encryption on PCD
  delay(500);
}

void music() {
  mp3_stop();
  display.clearDisplay();       //一度初期化
  display.setTextColor(WHITE);  //出力する文字の色
  display.setCursor(0, 0);      //カーソル位置の指定
  display.setTextSize(2);       // 出力する文字の大きさ
  display.println("MUSIC");
  display.display();  //ディスプレイ情報の更新
  delay(1500);

  String Mes1;
  String Mes2;
  byte playN = 6;
  bool mode = 0;

  while (phase == 0) {
    bool Deduct = digitalRead(DEDUCT_BTN);
    bool Add = digitalRead(ADD_BTN);
    bool submit = digitalRead(SUBMIT_BTN);
    byte vol = analogRead(A0) / 25;
    mp3_set_volume(vol);

    display.clearDisplay();   //一度初期化
    display.setCursor(0, 0);  //カーソル位置の指定
    display.setTextSize(2);   // 出力する文字の大きさ
    display.println(playN);
    display.setTextSize(1);  // 出力する文字の大きさ
    display.print("volume = ");
    display.println(vol);
    display.println(Mes2);
    display.display();  //ディスプレイ情報の更新

    if (Deduct == 0 && Add == 0) break;

    if (playN == 5) playN = 15;
    if (playN == 16) playN = 6;
    if (vol == 255) vol = 30;
    if (vol == 31) vol = 0;

    //    if (playN == 6) Mes1 = "6_maware";
    //    if (playN == 7) Mes1 = "7_gotoubun";
    //    if (playN == 8) Mes1 = "8_kimagure";
    //    if (playN == 9) Mes1 = "9_iwanaikedone";
    //    if (playN == 10) Mes1 = "10_chikatto";
    //    if (playN == 11) Mes1 = "11_do-n";
    //    if (playN == 12) Mes1 = "12_huzwarai";
    //    if (playN == 13) Mes1 = "13_suggo-i";
    //    if (playN == 14) Mes1 = "14_platina disco";
    //    if (playN == 15) Mes1 = "15_water blue";

    if (submit == 0 && Deduct == 1 && Add == 1) mode++;

    if (mode == 0) {
      mp3_stop();
      Mes2 = "NOT PLAYING";
      delay(50);
    }

    else {
      Mes2 = "PLAYING";
      // DFplayerでloop内では誘導音を流し続けるためのプログラム
      mp3_get_state();
      int state = mp3_wait_state();
      if (state == 510) {
        Serial.print("play ");
        Serial.println(playN);
        mp3_play(playN);
      }
      delay(300);
    }

    if (Add == 0 && Deduct == 1) {
      mode = 0;
      playN++;
      delay(500);
    }

    if (Deduct == 0 && Add == 1) {
      mode = 0;
      playN--;
      delay(500);
    }
  }
}

void PDis(String Message1, int Message2) {
  display.clearDisplay();       //一度初期化
  display.setTextColor(WHITE);  //出力する文字の色
  display.setCursor(0, 0);      //カーソル位置の指定
  display.setTextSize(2);       // 出力する文字の大きさ
  display.println(Message1);
  display.setTextSize(1);
  display.println(Message2);
  display.display();  //ディスプレイ情報の更新
}

void MDis(String Message1, String Message2, String Message3, String Message4) {
  display.clearDisplay();       //一度初期化
  display.setTextColor(WHITE);  //出力する文字の色
  display.setCursor(0, 0);      //カーソル位置の指定
  display.setTextSize(1);       // 出力する文字の大きさ
  display.println(Message1);
  display.println(Message2);
  display.println(Message3);
  display.println(Message4);
  display.display();  //ディスプレイ情報の更新
}

void dump_byte_array(byte* buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? "0" : "");
    Serial.print(buffer[i], HEX);
  }
  Serial.println();
}
