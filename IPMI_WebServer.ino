#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <Ethernet2.h>



#define OPT_ON    1
#define OPT_OFF   0
#define MAX_PAGE  5
#define IP_HOST   "192.168.10.1"
#define PWR_G     "green"
#define PWR_R     "red"

enum {
  STATE,
  INFO,
  TEMP,
  BEEP,
  RELAY,
  NOT_AVALIABLE
}eHomePage;

uint8_t Relay_PIN = 7; //D7
uint8_t Input_PIN = 4; //D4
uint8_t Beep_PIN = 9; //D9
uint8_t TEM_PIN = 8; //D8
uint8_t TEM_PWM_PIN = 0; //A0

uint8_t PressedCount = 0;
bool BackLight = OPT_OFF;
uint8_t PageIndex = 0;
bool IsClicked = false;
uint8_t TemAlarm = 40;
bool BeepByAlarm = false;
bool RelayState = false;

LiquidCrystal_I2C lcd(0x27,16,2);
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 10, 1);
EthernetServer server(80);

String LoginPageF1;
String LoginPageF2;
String LoginPageF3;


/********************Local Fucntions******************************/
uint8_t GetTEM(void)
{
  const uint16_t BETA = 3950; // should match the Beta Coefficient of the thermistor
  int analogValue = analogRead(TEM_PWM_PIN);
  uint8_t celsius = 1 / (log(1 / (1023. / analogValue - 1)) / BETA + 1.0 / 298.15) - 273.15;
  return celsius;
}

bool ButtonPressed(void)
{
  if(digitalRead(Input_PIN) == OPT_ON)
  {
    return OPT_OFF;
  }
  return OPT_ON;
}

void Beep(bool opt)
{
  if(opt == OPT_ON)
  {
    digitalWrite(Beep_PIN, LOW);
  }
  else
  {
    digitalWrite(Beep_PIN, HIGH);
  }
}

void Relay(bool opt)
{
  if(opt == OPT_ON)
  {
    digitalWrite(Relay_PIN, HIGH);
    RelayState = OPT_ON;
  }
  else
  {
    digitalWrite(Relay_PIN, LOW);
    RelayState = OPT_OFF;
  }
}

void ScrollPage()
{
  String tmpStr;

  lcd.clear();
  switch(PageIndex % MAX_PAGE)
  {
    case STATE:
      lcd.setCursor(0, 0);
      lcd.print("System Status:");
      lcd.setCursor(0, 1);
      lcd.print("  Running");
      break;
    case INFO:
      lcd.setCursor(0, 0);
      lcd.print("System Info:");
      lcd.setCursor(0, 1);
      Serial.println(Ethernet.localIP());
      tmpStr += IP_HOST;
      tmpStr.trim();
      tmpStr += ":";
      tmpStr += "80";
      lcd.print(tmpStr);
      break;
    case TEMP:
      lcd.setCursor(0, 0);
      lcd.print("TEMPERATURE:");
      lcd.setCursor(4, 1);
      tmpStr += String(GetTEM(), 1);
      tmpStr += " `C";
      lcd.print(tmpStr);
      if(GetTEM() > TemAlarm)
      {
        Relay(OPT_OFF);
        if (BeepByAlarm == true)
        {
          Beep(OPT_ON);
          delay(50);
          Beep(OPT_OFF);
        }
      }
      break;
    case BEEP:
      lcd.setCursor(0, 0);
      lcd.print("BEEP");
      lcd.setCursor(2, 1);
      lcd.print("RUNNING");
      Beep(OPT_ON);
      delay(50);
      Beep(OPT_OFF);
      break;
    case RELAY:
      lcd.setCursor(0, 0);
      lcd.print("RELAY:");
      lcd.setCursor(4, 1);
      if(RelayState == OPT_ON)
        lcd.print("ON");
      else
        lcd.print("OFF");
      break;
    default:
      break;
  }
  Serial.println(tmpStr);
  tmpStr = "";
}

void PressedCheck()
{
  delay(50);
  bool pressed = ButtonPressed();
  if(pressed == false && IsClicked == true) //clicked
  {
    PageIndex ++;
    IsClicked = false;
    ScrollPage();
  }
  if(pressed)
  {
    IsClicked = true;
    PressedCount ++;
    if(PressedCount > 20)
    {
      PressedCount = 0;
      BackLight = !BackLight;
      lcd.setBacklight(BackLight);
      delay(200);
    }
  }
  if (PageIndex % MAX_PAGE == TEMP || 
      PageIndex % MAX_PAGE == BEEP)
  {
    ScrollPage();
  }
}

void HTMLFresh()
{
  LoginPageF1 = "";
  LoginPageF2 = "";
  LoginPageF3 = "";
  LoginPageF1 += "<!DOCTYPE html><html><head><title>IPMI Status</title><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style>label{display:inline-block;width:150px;text-align:right;margin-right:10px;}</style></head><body><h1>IPMI Status</h1><label>SYS STAT：</label><label>Normal</label><br><label>TEMP: </label><label id=\"temp\">";
  LoginPageF1 += GetTEM();
  LoginPageF1 += "</label><br><label>Power State: </label><button id=\"power\" onclick=\"updatePower()\" style=\"background:";
  LoginPageF1 += (RelayState ? PWR_G : PWR_R);
  Serial.println(LoginPageF1); 
  LoginPageF2 += "\"></button><br><label>Temp Alarm Limit </label><input id=\"limit\" value=\"";
  LoginPageF2 += TemAlarm;
  LoginPageF2 += "\"><button onclick=\"updateLimit()\">Apply</button></body><script>var p=document.getElementById(\"power\"),l=document.getElementById(\"limit\"),s=true;";
  LoginPageF2 += "p.onclick=function(){s=!s;p.style.background=s?\"green\":\"red\";";
  LoginPageF2 += "var x=new XMLHttpRequest();x.open(\"POST\",\"#\");";
  Serial.println(LoginPageF2);
  LoginPageF3 += "x.setRequestHeader(\"Content-Type\",\"application/1\");";
  LoginPageF3 += "x.send(\"powerState=\"+s);};function updateLimit(){var x=new XMLHttpRequest();";
  LoginPageF3 += "x.open(\"POST\",\"#\");x.setRequestHeader(\"Content-Type\",\"application/1\");";
  LoginPageF3 += "x.send(\"tempLimit=\"+l.value);}</script></html>";
  Serial.println(LoginPageF3); 
}

void ServerLoop()
{
  HTMLFresh();
  EthernetClient client = server.available();
  if (client) {
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println(LoginPageF1);
          client.println(LoginPageF2);
          client.println(LoginPageF3);
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(Input_PIN, INPUT);
  pinMode(Relay_PIN, OUTPUT);
  pinMode(Beep_PIN, OUTPUT);
  pinMode(TEM_PIN, INPUT);
  pinMode(TEM_PWM_PIN, INPUT);
  lcd.init();
  lcd.clear();
  Beep(OPT_OFF);
  Relay(OPT_OFF);
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  PressedCheck();
  ServerLoop();
  delay(100); 
}
