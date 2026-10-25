#pragma once

/* PMU registers used to tell the Fullhan generations apart (same map as ipctool). */
#define FH_PMU_BASE     0xf0000000
#define FH_PMU_CHIP_ID  (FH_PMU_BASE + 0x00)
#define FH_PMU_IP_VER   (FH_PMU_BASE + 0x04)
#define FH_PMU_SYS_CTRL (FH_PMU_BASE + 0x0c)

/* Illuminators: /proc/driver/pwm takes "id,output_mask,duty_ns,period_ns,...",
 * the mask written raw to GLOBAL_CTRL2 (so 1 << id). IR = PWM3 (GPIO7), white
 * = PWM7 + the white-lamp GPIO. */
#define FH_PWM_ON(id, mask)  id "," mask ",60,200,0,0,0,0"
#define FH_PWM_OFF(id, mask) id "," mask ",0,200,0,0,0,0"
#define FH_LAMP_WHITE_GPIO 11
