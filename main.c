

/* === main.c : Weak Auth UX (LCD엔 입력/결과만, '*'=Backspace)
   ... (상단 동일)
*/
#define F_CPU 14745600UL
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <stdbool.h>
#include "lcd.h"

/* ===================== Keypad ===================== */
static inline void keypad_init(void){
    DDRD = 0x70;      // PD6..PD4=출력(컬럼), PD3..PD0=입력(행)
    PORTD = 0x00;     // 풀업 미사용(열 HIGH가 행으로 전달)
}
static inline void col_select(uint8_t col){       // col:0,1,2 -> PD4,5,6
    PORTD &= ~(0x70);
    PORTD |= (1 << (4 + col));                    // 해당 열만 HIGH
}
static char keypad_getkey_once(void){
    static const char keymap[4][3] = {
        {'1','2','3'}, {'4','5','6'}, {'7','8','9'}, {'*','0','#'}
    };
    for(uint8_t c=0;c<3;c++){
        col_select(c); _delay_us(5);
        uint8_t rows = PIND & 0x0F;               // PD3..PD0
        for(uint8_t r=0;r<4;r++){
            uint8_t m = (1<<r);
            if(rows & m){
                _delay_ms(10);
                if(PIND & m){
                    while(PIND & m){ _delay_ms(1); } // 릴리즈 대기
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
static inline void leds_init(void){ LED_DDR=0xFF; LED_PORT=0x00; }
static inline void leds_all_on(void){ LED_PORT=0xFF; }
static inline void leds_all_off(void){ LED_PORT=0x00; }
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
static const char FIXED_PIN[] = "0258";
static inline bool check_pin_4(const char *typed){ return strncmp(typed, FIXED_PIN, 4)==0; }

/* ===================== main ===================== */
int main(void){
    Portinit();
    LCD_init();
    keypad_init();
    leds_init();
    uart_init();

    uart_println("=== UART Demo: Weak Auth UX ===");
    uart_println("[VULN] Hardcoded PIN (leaked): 0258");
    uart_println("[VULN] Plaintext echo, no lockout");

    // 초기 화면
    LCD_pos(0,0); LCD_STR("PIN: ");
    LCD_pos(1,0); LCD_STR("                "); // 2행 비움(첫 시작만)

    char buf[5]={0};
    uint8_t idx=0;

    while(1){
        char k = keypad_getkey_once();
        if(!k){ _delay_ms(3); continue; }

        /* ===== 제출: '#' ===== */
        if(k=='#'){
            buf[idx]='\0';
            if(idx==4 && check_pin_4(buf)){
                leds_all_on();
                LCD_pos(1,0); LCD_STR("OK              ");    // 결과 유지
                uart_print("[RESULT] OK, PIN="); uart_println(buf);
            } else {
                leds_blink_times(6); leds_all_off();
                LCD_pos(1,0); LCD_STR("FAIL            ");     // 결과 유지
                uart_print("[RESULT] FAIL, PIN="); uart_println(buf);
            }
            // 입력만 초기화 (화면은 유지)
            idx=0; buf[0]='\0';
            // ★ 변경: 아래 두 줄 삭제(화면에 입력값을 유지)
            // LCD_pos(0,0); LCD_STR("PIN: ");
            // LCD_pos(0,5); LCD_STR("    ");
            continue;
        }

        /* ===== 백스페이스: '*' ===== */
        if(k=='*'){
            if(idx>0){ idx--; buf[idx]='\0'; }
            LCD_pos(0,0); LCD_STR("PIN: ");
            LCD_pos(0,5); LCD_STR("    ");
            LCD_pos(0,5); LCD_STR(buf);
            continue;
        }

        /* ===== 숫자 입력(중복 허용, 최대 4자리) ===== */
        if(k>='0' && k<='9' && idx<4){
            // 새 입력의 '첫 글자'면 이전 결과 + 입력칸 모두 지우기
            if(idx==0){
                LCD_pos(1,0); LCD_STR("                ");   // 결과줄 클리어
                LCD_pos(0,5); LCD_STR("    ");               // ★ 입력칸도 클리어(새 입력 시작)
            }
            buf[idx++]=k;
            uart_putc(k);                                     // 취약: 평문 에코
            LCD_pos(0,0); LCD_STR("PIN: ");
            LCD_pos(0,5); LCD_STR(buf);

            /* (옵션) 4자리 자동 검사 */
            if(idx==4){
                if(check_pin_4(buf)){
                    leds_all_on();
                    LCD_pos(1,0); LCD_STR("OK              ");  // 결과 유지
                    uart_print("[RESULT] OK, PIN="); uart_println(buf);
                } else {
                    leds_blink_times(6); leds_all_off();
                    LCD_pos(1,0); LCD_STR("FAIL            ");   // 결과 유지
                    uart_print("[RESULT] FAIL, PIN="); uart_println(buf);
                }
                // 입력만 초기화 (화면 유지)
                idx=0; buf[0]='\0';
                // ★ 변경: 아래 두 줄 삭제(화면에 입력값을 유지)
                // LCD_pos(0,0); LCD_STR("PIN: ");
                // LCD_pos(0,5); LCD_STR("    ");
            }
        }
    }
}

