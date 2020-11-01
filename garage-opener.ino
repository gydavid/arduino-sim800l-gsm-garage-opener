#include <SoftwareSerial.h>
#include <EEPROM.h>
/*
 * SMS commands:
 * "add +36209999999 password"
 * "del +36209999999 password" or "delete +36209999999 password"
 * "list password"
 * "?" or "help"
 */

String password = "password";

const int SIM800_RX = 10;
const int SIM800_TX = 11;
const int door1 = 2;
const int door2 = 3;
const int button = 2;

const int phoneNumberSize = 12;
const int maxPhoneNumbers = 10;
String storedNumbers[maxPhoneNumbers];

SoftwareSerial SIM800(SIM800_RX, SIM800_TX);

void setup() {
  Serial.begin(9600);
  Serial.println("Setup begins");
  pinMode(door1, OUTPUT);
  pinMode(door2, OUTPUT);
  pinMode(button, INPUT);
//  initPhone();
  readStoredNumbers(); // required!
  Serial.println("Setup end");
}

void loop() {
  String response;
  bool knownMessage;
  byte firstChar;
  if (SIM800.available()) {
    response = gsmCommand("");
    firstChar = response[0];
    if (firstChar==255) {
      initPhone();
    } else if (response.startsWith("RING")) {
      ring(response);
    } else if (response.startsWith("+CMTI:")) {
      readSMS();
    } else if (response == "OK") {
    } else {
      Serial.println(response);
    }
  }

  if (Serial.available() > 0) {
    Serial.println("Console command:");
    String command = Serial.readString();
    Serial.println(command);
    command.replace("\n", "");
    command.replace("\r", "");
    Serial.println(smsCommand(command + " " + password));
  }

  if (digitalRead(button) == HIGH) {
    openDoors();
  }
}

void initPhone() {
  SIM800.begin(9600);
  Serial.println(gsmCommand("ATZ"));
  Serial.println(gsmCommand("AT+CMGF=1"));
  Serial.println(gsmCommand("AT+IPR=9600"));
  Serial.println(gsmCommand("AT+CSCLK=0")); // enable sleep mode
  Serial.println(gsmCommand("AT+CLIP=1"));
  Serial.println(gsmCommand("AT&W"));
}

void readSMS() {
  String sms = gsmCommand("AT+CMGR=1");
  String message = parseSMSMessage(sms);
  String sender = parseSMSSender(sms);
  String respond = smsCommand(message);
  if(respond != "") {
    Serial.println(gsmCommand("AT+CMGDA=\"DEL ALL\""));
    for (int i = 0; i < 11; i++ ) {
      Serial.println(gsmCommand("AT+CMGD=" + String(i)));
    }
    delay(200);
    sendSMS(sender, respond);
  }
}

void sendSMS(String number, String message) {
  message.replace("\n", " ");
  message.replace("\r", "");
  Serial.println("Send SMS To: " + number);
  Serial.println("SMS Message: " + message);
  Serial.println(gsmCommand("AT+CMGF=1"));
  Serial.println(gsmCommand("AT+CMGS=\"" + number + "\""));
  Serial.println(gsmCommandStrict(message));
  Serial.println(gsmCommandStrictChar((char)26));
}

String parseSMSMessage(String sms) {
  int startNumber = sms.indexOf("\",\"") + 3;
  int startMessage = sms.indexOf("\n", startNumber) + 1;
  int endMessage = sms.indexOf("\n", startMessage);
  String message = sms.substring(startMessage, endMessage);
  return message;
}

String parseSMSSender(String sms) {
  int startNumber = sms.indexOf("\",\"") + 3;
  String sender = sms.substring(startNumber, startNumber + phoneNumberSize);
  return sender;
}

String smsCommand(String message) {
  int passwordPos = message.indexOf(" " + password);
  if (passwordPos > -1) {
    String command = message;
    command.toLowerCase();
    if (command.startsWith("add ")) {
      return putNumber(getNumberFromCommand(message));
    } else if (command.startsWith("del ")) {
      return removeNumber(getNumberFromCommand(message));
    } else if (command.startsWith("delete ")) {
      return removeNumber(getNumberFromCommand(message));
    } else if (command.startsWith("list ")) {
      return listNumbers();
    } else {
      return "";
    }
  } else {
    return "INVALID PASSWORD";
  }
}

String listNumbers() {
  String response = "Stored numbers: ";
  for (int i = 0; i < maxPhoneNumbers; i++) {
    if (storedNumbers[i] != "") {
      response = response + storedNumbers[i] + ", "; 
    }
  }
  return response;
}

String getNumberFromCommand(String message) {
  int numberPos = message.indexOf(" ");
  int numberEnd = message.indexOf(" ", numberPos + 1);
  String number = message.substring(numberPos + 1, numberEnd);
  return number;
}

String getCaller(String response) {
  String caller="???";
  if (response.indexOf("+CLIP",0) > -1) {
    int quota1 = response.indexOf('"');
    int quota2 = response.indexOf('"', quota1 + 1);
    caller = response.substring(quota1+1, quota2);
  }
  return caller;
}

void openDoors() {
  Serial.println("Open doors");
  digitalWrite(door1 , HIGH);
  delay(300);
  digitalWrite(door1, LOW);
  delay(1500);  
  digitalWrite(door2 , HIGH);
  delay(300);
  digitalWrite(door2, LOW);  
}

void ring(String response) {
  String caller;
  
  caller = getCaller(response);
  Serial.println("Caller: "+caller);
  Serial.println(gsmCommand("ATH"));
  
  if (checkNumber(caller)) {
    openDoors();
  }
}

String gsmCommand(String command) {
  return commandw(command, true);
}

String gsmCommandStrict(String command) {
  return commandw(command, false);
}

String gsmCommandStrictChar(char command) {
  return commandwChar(command, false);
}

String commandwChar(char command, boolean line) {
  int lc=500;
  String response;
  char incoming_char=0;
  
  if (command != "") {
    Serial.println("Sending command: " + command);
    if(line) {
      SIM800.println(command);
    } else {
      SIM800.print(command);
    }
    Serial.println("Sent. Waiting response");
  } else {
    Serial.println("Receiving data");
  }
  do {
    delay(100);
    lc--;
    if (SIM800.available()) {
      do {
        incoming_char = SIM800.read();
        response += incoming_char;
      } while (SIM800.available());
      lc = 0;
    }
  } while ( lc > 1 );
  response.trim();
  return response;
}

String commandw(String command, boolean line) {
  int lc=500;
  String response;
  char incoming_char=0;
  
  if (command != "") {
    Serial.println("Sending command: " + command);
    if(line) {
      SIM800.println(command);
    } else {
      SIM800.print(command);
    }
    Serial.println("Sent. Waiting response");
  } else {
    Serial.println("Receiving data");
  }
  do {
    delay(100);
    lc--;
    if (SIM800.available()) {
      do {
        incoming_char = SIM800.read();
        response += incoming_char;
      } while (SIM800.available());
      lc = 0;
    }
  } while ( lc > 1 );
  response.trim();
  return response;
}

void clearEEPROM() {
  for (int i = 0 ; i < EEPROM.length() ; i++ ) {
    EEPROM.write(i, 0);
  }
  Serial.print("EEPROM deleted, size:");
  Serial.println(EEPROM.length());
}

String putNumber(String number) {
  if (checkNumber(number)) {
    return "ALREADY EXITS NUMBER";
  }
  char numberChar[phoneNumberSize + 1];
  number.toCharArray(numberChar, phoneNumberSize + 1);
  int target = firstEmptyNumber();
  EEPROM.put( target * (phoneNumberSize + 1), numberChar);
  readStoredNumbers();
  return "OK";
}

String removeNumber(String number) {
  if (!checkNumber(number)) {
    return "NOT EXITS NUMBER";
  }
  for (int i = 0; i < maxPhoneNumbers; i++) {
    if (storedNumbers[i] == number) {
      int from = i * (phoneNumberSize + 1);
      int to = from + (phoneNumberSize + 1);
      for (int m = from; m < to; m++ ) {
        EEPROM.write(m, 0);
      }
    }
  }
  readStoredNumbers();
  return "OK";
}

boolean checkNumber(String number) {
  boolean match = false;
  for (int i = 0; i < maxPhoneNumbers; i++) {
    if (storedNumbers[i] == number) {
      match = true;
    }
  }
  return match;
}

void readStoredNumbers() {
  for( int i = 0; i < maxPhoneNumbers;  ++i ) {
    storedNumbers[i] = "";
  }
  int f = 0;
  int m = 0;
  char number[phoneNumberSize + 1];
  Serial.println("Stored numbers:");
  do {
    EEPROM.get( f * (phoneNumberSize + 1), number);
    if (String(number[0]) == "+") {
      storedNumbers[m] = String(number);
      Serial.println(number);
      m++;
    }
    f++;
  } while (f < maxPhoneNumbers);
}

int firstEmptyNumber() {
  int empty = 100;
  int f = 0;
  char number[phoneNumberSize + 1];
  do {
    EEPROM.get( f * (phoneNumberSize + 1), number);
    if (String(number[0]) != "+") {
      empty = f;
      break;
    }
    f++;
  } while (f < maxPhoneNumbers);
  return empty;
}
