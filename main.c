/* === main.c : Weak Auth UX (LCD엔 입력/결과만, '*'=Backspace)
   키패드: ROW=PD0~PD3(입력), COL=PD4~PD6(출력, 활성-HIGH)  ← keypadTest2 방식과 동일
   LCD:   네가 쓰는 lcd.h/Portinit() API 그대로 사용
   LED:   PORTA
   UART0: 9600 8N1 (하드코딩 PIN 노출 + 평문 에코 + 결과 로그)
================================================================ */
#define F_CPU 14745600UL
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <stdbool.h>
#include "lcd.h"

/* ===================== Keypad (Port D 한 포트) ===================== */
/* PD4~PD6 = 컬럼(출력), PD0~PD3 = 로우(입력). 눌림 = 행 HIGH 감지 */
static inline void keypad_init(void){
    DDRD = 0x70;      // PD6..PD4 = 1(출력), PD3..PD0 = 0(입력)
    PORTD = 0x00;     // 풀업 사용 안 함(행은 부동; 눌릴 때 열 HIGH가 전달됨)
}

/* 열을 하나만 HIGH로 올리고 해당 열에서 행 HIGH를 감지 */
static inline void col_select(uint8_t col){ // col: 0,1,2 -> PD4,PD5,PD6
    PORTD &= ~(0x70);                // 상위 3비트 초기화
    PORTD |= (1 << (4 + col));       // 선택 열만 HIGH
}

/* 한 번 스캔: 입력된 키 문자(‘0’~‘9’, ‘*’, ‘#’) 또는 0 반환 */
static char keypad_getkey_once(void){
    static const char keymap[4][3] = {
        {'1','2','3'},
        {'4','5','6'},
        {'7','8','9'},
        {'*','0','#'}
    };
    for(uint8_t c=0;c<3;c++){
        col_select(c);              // 현재 열만 HIGH
        _delay_us(5);
        uint8_t rows = PIND & 0x0F; // PD3..PD0

        // 활성-HIGH: 해당 행 비트가 1이면 눌림
        for(uint8_t r=0;r<4;r++){
            uint8_t mask = (1<<r);
            if(rows & mask){
                _delay_ms(10);                  // 디바운스
                if(PIND & mask){
                    while(PIND & mask){ _delay_ms(1); } // 릴리즈 대기
                    return keymap[r][c];
                }
            }
        }
    }
    return 0;
}

/* ===================== LED ===================== */
#define LED_PORT PORTA
#define LED_DDR  DDRA
static inline void leds_init(void){ LED_DDR = 0xFF; LED_PORT = 0x00; }
static inline void leds_all_on(void){ LED_PORT = 0xFF; }
static inline void leds_all_off(void){ LED_PORT = 0x00; }
static void leds_blink_times(uint8_t n){
    for(uint8_t i=0;i<n;i++){ LED_PORT=0xFF; _delay_ms(150); LED_PORT=0x00; _delay_ms(150); }
}

/* ===================== UART0 (9600 8N1) ===================== */
static void uart_init(void){
    const uint16_t ubrr = 95; // 14.7456MHz → 9600bps
    UBRR0H=(uint8_t)(ubrr>>8); UBRR0L=(uint8_t)(ubrr&0xFF);
    UCSR0A=0x00; UCSR0B=(1<<TXEN0); UCSR0C=(1<<UCSZ01)|(1<<UCSZ00);
}
static inline void uart_putc(char c){ while(!(UCSR0A&(1<<UDRE0))); UDR0=c; }
static inline void uart_print(const char *s){ while(*s) uart_putc(*s++); }
static inline void uart_println(const char *s){ uart_print(s); uart_print("\r\n"); }

/* ===================== Auth (취약 데모) ===================== */
static const char FIXED_PIN[] = "1234";
static inline bool check_pin_4(const char *typed){ return strncmp(typed, FIXED_PIN, 4)==0; }

/* ===================== main ===================== */
int main(void){
    // LCD는 네 라이브러리 그대로
    Portinit();      // (예시 주석) PB 데이터버스, PG 제어 라인 초기화
    LCD_init();

    keypad_init();
    leds_init();
    uart_init();

    // 취약점 배너는 UART로만
    uart_println("=== UART Demo: Weak Auth UX ===");
    uart_println("[VULN] Hardcoded PIN (leaked): 1234");
    uart_println("[VULN] Plaintext echo, no lockout");

    // LCD 초기: 1행에 입력만, 2행 비움
    LCD_pos(0,0); LCD_STR("PIN: ");
    LCD_pos(1,0); LCD_STR("                "); // 16칸 지우기

    char buf[5]={0}; uint8_t idx=0;

    while(1){
        char k = keypad_getkey_once();
        if(!k){ _delay_ms(3); continue; }

        /* ===== 제출(엔터) 처리: '#' ===== */
        if(k=='#'){
            buf[idx]='\0';
            if(idx==4 && check_pin_4(buf)){
                leds_all_on();
                LCD_pos(1,0); LCD_STR("OK              ");
                uart_print("[RESULT] OK, PIN="); uart_println(buf); // ★ UART 결과 로그
                _delay_ms(800);
            } else {
                leds_blink_times(6); leds_all_off();
                LCD_pos(1,0); LCD_STR("FAIL            ");
                uart_print("[RESULT] FAIL, PIN="); uart_println(buf); // ★ UART 결과 로그
                _delay_ms(800);
            }
            // 초기화
            idx=0; buf[0]='\0';
            LCD_pos(0,0); LCD_STR("PIN: ");
            LCD_pos(0,5); LCD_STR("    ");          // 4칸 지우기
            LCD_pos(1,0); LCD_STR("                ");
            continue;
        }

        /* ===== 백스페이스: '*' ===== */
        if(k=='*'){
            if(idx>0){ idx--; buf[idx]='\0'; }
            LCD_pos(0,0); LCD_STR("PIN: ");
            LCD_pos(0,5); LCD_STR("    ");          // 4칸 지우고
            LCD_pos(0,5); LCD_STR(buf);             // 남은 입력 다시 쓰기
            continue;
        }

        /* ===== 숫자: 중복 허용, 최대 4자리 ===== */
        if(k>='0' && k<='9' && idx<4){
            buf[idx++]=k;
            uart_putc(k); // 평문 에코(취약)
            LCD_pos(0,0); LCD_STR("PIN: ");
            LCD_pos(0,5); LCD_STR("    ");          // 4칸 지움
            LCD_pos(0,5); LCD_STR(buf);

            /* (옵션) ★ 4자리 자동 검사: '#' 안 눌러도 바로 결과 표시 */
            if(idx==4){
                if(check_pin_4(buf)){
                    leds_all_on();
                    LCD_pos(1,0); LCD_STR("OK              ");
                    uart_print("[RESULT] OK, PIN="); uart_println(buf); // ★ UART 결과
                } else {
                    leds_blink_times(6); leds_all_off();
                    LCD_pos(1,0); LCD_STR("FAIL            ");
                    uart_print("[RESULT] FAIL, PIN="); uart_println(buf); // ★ UART 결과
                }
                _delay_ms(800);
                // 초기화
                idx=0; buf[0]='\0';
                LCD_pos(0,0); LCD_STR("PIN: ");
                LCD_pos(0,5); LCD_STR("    ");
                LCD_pos(1,0); LCD_STR("                ");
            }
        }
    }
}
