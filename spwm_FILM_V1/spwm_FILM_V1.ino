#include <avr/io.h>
#include <avr/interrupt.h>

/*
   Left bridge used for fundamental signal (50Hz/60Hz), Right bridge for SPWM (10kHz carrier freq.)
   Sampling per Cycle 
   ---> 10kHz/50Hz = 200 
   ---> 10kHZ/60hz = 166
   Look Up table entries use only half cycles (identical positive and negative cycles) 
   50 Hz --->  200/2 = 100 entries
   60 Hz --->  167/2 = 83 entries
   
   SPWM clock = fXTAL/freq. carrier = 16.000.000/10.000 = 1.600 clock. 
   WGM mode 8 is used, so ICR1 = 1.600/2 = 800 clk
   Look up tables for a half cycle (100 or 83 entries), max value = 800 (100% duty cycle)is loaded into register ICR1.

  This code is for 50Hz !!!
  for 60Hz use the Lookup Table for 60Hz and use the code marked on the ISR(TIMER1_OVF_vect) !!!
*/
#define f_xtel 16000000
#define f_spwm  10000
#define f_sin  50

#define Cycle (f_spwm/f_sin)
#define point1   Cycle/2
int point = point1;
int clk=(f_xtel/f_spwm)/2;
double Amplitude;
int phs;
int Vo;
int set_sin[Cycle+1] ={};

void setup() {
  pinMode(2, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(3, OUTPUT);
  digitalWrite(2, LOW);
  digitalWrite(12, HIGH);
  Serial.begin(115200);
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  double angle;
  
  for (int i = 0; i <= point; i++) {
    angle = i * M_PI / point;
    set_sin[i] = int(sin(angle) * clk);
    // Serial.println(set_sin[i]);
    
  }
  


  // Register initilisation, see datasheet for more detail.
  TCCR1A = 0b10110000;
  TCCR1B = 0b00010001;
  TIMSK1 = 0b00000001;
  ICR1 = clk;        /* Counter TOP value (at 16MHz XTAL, SPWM carrier freq. 10kHz, 200 samples/cycle).*/
  sei();             /* Enable global interrupts.*/
  DDRB = 0b00011110; /* Pin 9, 10, 11, 12 as outputs.*/
  PORTB = 0;




  Amplitude = 0.01;
}


// void alarmIndication(int alarm) {
//   TCCR1A = 0;  // shutdown SPWM output
//   TIMSK1 = 0;
//   PORTB &= 0b11100001;
// loopX:
//   for (int i = 0; i < alarm; i++) {
//     digitalWrite(7, HIGH);  // turn ON LED and Buzzer
//     digitalWrite(13, HIGH);
//     delay(200);
//     digitalWrite(7, LOW);  // then turn OFF
//     digitalWrite(13, HIGH);
//     delay(200);
//   }
//   delay(1000);
//   goto loopX;  //never ending story... until reset
// }
int start = 0;
int d = 0;
int li = 1;
unsigned long period = 50;    //ระยะเวลาที่ต้องการรอ
unsigned long last_time = 0;  //ประกาศตัวแปรเป็น global เพื่อเก็บค่าไว้ไม่ให้ reset จากการวนloop
double add_sub = 0.002;
double min = 0.10;
double max = 0.70;
void loop() {
  Serial.print("point = ");
  Serial.print(point);
  Serial.print(" point1 = ");
  Serial.print(point1);
  Serial.print(" clk = ");
  Serial.print(clk);
  Serial.print(" Cycle = ");
  Serial.print(Cycle+1);
  Serial.println();

  if (start <= 0) {
    delay(5000);
    start++;
    digitalWrite(2, HIGH);
    digitalWrite(12, LOW);
    // while(Amplitude<0.40){
    //    Amplitude = Amplitude + add_sub;
    //    delay(period);
    // }
  }

  if (millis() - last_time > period) {
    last_time = millis();  //เซฟเวลาปัจจุบันไว้เพื่อรอจนกว่า millis() จะมากกว่าตัวมันเท่า period

    int volt_set = analogRead(A1);
    Vo=10;
    volt_set=20;

    // Serial.print(volt_set);
    // Serial.print(" ");
    // Serial.print(Vo);
    // Serial.print(" ");
    // Serial.print(Amplitude);
    // Serial.println(" ");

    if ((Vo + li) < volt_set) {
      Amplitude = Amplitude + add_sub;
      digitalWrite(13, !digitalRead(13));
    } else if ((Vo - li) > volt_set) {
      Amplitude = Amplitude - add_sub;
      digitalWrite(13, !digitalRead(13));
    } else {
      digitalWrite(13, HIGH);
    }
    if (Amplitude <= min) Amplitude = min;
    if (Amplitude >= max) Amplitude = max;
  }
}
// if (num == point / 2 && ph != 1) {
  //   Vo = analogRead(0);
  // }
ISR(TIMER1_OVF_vect) {
  static int num;
  static int  ph;
  static int dtA = 0;
  static int dtB = 5;

  if (num >= point-1) {    // <------------------ 50 Hz !!!
    if (ph == 0) {         // OC1A as SPWM out
      TCCR1A = 0b10110000; // clear OC1A, set OC1B on compare match
      dtA = 0;             // no dead time
      dtB = 5;            // adding dead time to OC1B
    } else {
      TCCR1A = 0b11100000; // OC1B as SPWM out
      dtA = 5;
      dtB = 0;
    }
    ph ^= 1;
  }
  OCR1A = int(set_sin[num] * Amplitude) + dtA; // SPWM width update
  OCR1B = int(set_sin[num] * Amplitude) + dtB; // note: 0.7 used to reduce inveter output voltage

  num++;
  if (num >= point) {                   // toggle left bridge (50Hz) !!! 
    delayMicroseconds(60);
    if (ph == 1) {
      digitalWrite(3, LOW);
      // delayMicroseconds(100);
      digitalWrite(11, HIGH);
      phs = 1;
    } else {
      digitalWrite(11, LOW);
      // delayMicroseconds(100);
      digitalWrite(3, HIGH);
      phs = 0;
    }
    num = 0;
  }
}   