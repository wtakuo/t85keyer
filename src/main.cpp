/*
 * ATTiny85 Electronic Keyer
 * Copyright (C) by Takuo Watanabe (JM1TEU), 2023
 */

#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#define DIT_PIN PB3
#define DAH_PIN PB4
#define POT_PIN A1   // PB2
#define KEY_PIN PB0
#define BUZ_PIN PB1

#define TONE_FREQ 600

#define IPU 4
int wpm = 20;
int ti = (1200 / IPU) / wpm;

void sleep() {
    GIMSK |= _BV(PCIE);
    PCMSK |= _BV(PCINT3);
    PCMSK |= _BV(PCINT4);
    ADCSRA &= ~_BV(ADEN);
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sei();
    sleep_cpu();

    cli();
    PCMSK &= ~_BV(PCINT3);
    PCMSK &= ~_BV(PCINT4);
    sleep_disable(); 
    ADCSRA |= _BV(ADEN);
    sei();
}

ISR(PCINT0_vect) {}

enum state { st_idle, st_dit, st_dah, st_space };

static inline void key_on(void) {
    tone(BUZ_PIN, TONE_FREQ);
    digitalWrite(KEY_PIN, HIGH);
}

static inline void key_off(void) {
    noTone(BUZ_PIN);
    digitalWrite(KEY_PIN, LOW);
}

void iter() {
    static enum state state = st_idle;
    static enum state next = st_idle;
    static enum state prev = st_idle;
    static uint8_t ci = 3 * IPU;
    static uint8_t pot = 64, pot1 = 64;

    uint8_t dit = !digitalRead(DIT_PIN);
    uint8_t dah = !digitalRead(DAH_PIN);

    switch (state) {
    case st_idle:
        if (dit) {
            key_on();
            ci = 1 * IPU;
            state = st_dit;
        }
        else if (dah) {
            key_on();
            ci = 3 * IPU;
            state = st_dah;
        }
        else {
            ci--;
            if (ci == 0) sleep();
        }
        break;
    case st_dit:
        pot = analogRead(POT_PIN) / 8;
        ci--;
        if (ci == 0) {
            key_off();
            prev = st_dit;
            ci = 1 * IPU;
            state = st_space;
        }
        if (dah) {
            next = st_dah;
        }
        break;
    case st_dah:
        pot = analogRead(POT_PIN) / 8;
        ci--;
        if (ci == 0) {
            key_off();
            prev = st_dah;
            ci = 1 * IPU;
            state = st_space;
        }
        if (dit) {
            next = st_dit;
        }
        break;
    case st_space:
        ci--;
        if (ci == 0) {
            if (next == st_dit) {
                key_on();
                next = st_idle;
                ci = 1 * IPU;
                state = st_dit;
            }
            else if (next == st_dah) {
                key_on();
                next = st_idle;
                ci = 3 * IPU;
                state = st_dah;
            }
            else {
                ci = 3 * IPU;
                state = st_idle;
            }
        }
        else if (prev == st_dit && dah) {
            next = st_dah;
        }
        else if (prev == st_dah && dit) {
            next = st_dit;
        }
        break;
    }

    if (pot != pot1) {
        wpm = 36 * pot / 128 + 5;
        ti = (1200 / IPU) / wpm;
    }
    pot1 = pot;
}

void setup() {
    pinMode(DIT_PIN, INPUT);
    pinMode(DAH_PIN, INPUT);
    // pinMode(POT_PIN, INPUT);
    pinMode(KEY_PIN, OUTPUT);
    pinMode(BUZ_PIN, OUTPUT);
    sleep();
}

void loop() {
    static long t0 = millis();
    long t = millis();
    if (t - t0 >= ti) {
        iter();
        t0 = t;
    }
}
