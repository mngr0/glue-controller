#include <atmel_start.h>
#include <ff.h>
#include "rtos_start.h"
#include "atmel_start.h"
#include "mpu_wrappers.h"

#define TASK_EXAMPLE_STACK_SIZE (128 / sizeof(portSTACK_TYPE))
#define TASK_EXAMPLE_STACK_PRIORITY (tskIDLE_PRIORITY + 1)
static TaskHandle_t      xCreatedExampleTask;

FATFS FatFs;		/* FatFs work area needed for each volume */
FIL Fil;			/* File object needed for each open file */



/*
index 0
over PB05
pwm PB09 PWM 0

index 1
over PB04
pwm PA07 PWM1

*/
void static open_over(uint8_t index){
	if (index){
		gpio_set_pin_level(PB04,1);
	}
	else{
		gpio_set_pin_level(PB05,1);
		
	}
}
void static open_normal(uint8_t index) {
	if (index){
		gpio_set_pin_level(PA07,1);
		}else{
		gpio_set_pin_level(PB09,1);
	}
}

void static open_under(uint8_t index) {
	
	if (index){
		PWM_1_init();
		pwm_set_parameters(&PWM_1,  1000,  500);
		pwm_enable(&PWM_1);
		}else{
		PWM_0_init();
		pwm_set_parameters(&PWM_0,  1000,  500);
		pwm_enable(&PWM_0);
	}
}

void static close_over(uint8_t index){
	if (index){
		gpio_set_pin_level(PB04,0);
		}else{
		gpio_set_pin_level(PB05,0);
	}
}


void static close_normal(uint8_t index){
	if (index){
		gpio_set_pin_level(PA07,0);
		
		}else{
			gpio_set_pin_level(PB09,0);
	}
}

void static close_under(uint8_t index){
	if (index){
		pwm_disable(&PWM_1);
		gpio_set_pin_direction(PA07, GPIO_DIRECTION_OUT);
		gpio_set_pin_function(PA07, GPIO_PIN_FUNCTION_OFF);
		gpio_set_pin_level(PA07,0);
		}else{
		pwm_disable(&PWM_0);
		gpio_set_pin_direction(PB09, GPIO_DIRECTION_OUT);
		gpio_set_pin_function(PB09, GPIO_PIN_FUNCTION_OFF);
		gpio_set_pin_level(PB09,0);
	}
}






#define max_len 64 //len of the time array



uint8_t active0=0;
uint8_t active1=0;

typedef struct conf_t{
	char name[16];
	uint32_t overtime;
	uint32_t undertime;
	uint32_t times[max_len];
	uint32_t len;
}conf;

conf confs[8];


void preparesd(){
	uint8_t k;
	for (k=0;k<8;k++){
		confs[k].name[0]='c';
		confs[k].name[1]=48+k;
		confs[k].overtime=0;
		confs[k].undertime=3000;
		confs[k].times[0]=(k+1)*1000;
		confs[k].times[1]=1000;
		confs[k].times[2]=5000;
		confs[k].times[3]=1000;
		confs[k].times[4]=5000;
		confs[k].times[5]=1000;
		confs[k].len=2;
	}
	
	UINT bw;
	uint32_t buff[ sizeof(conf)/4];
	f_mount(&FatFs, "", 0);
	if (f_open(&Fil, "config.txt",  FA_WRITE) == FR_OK) {
		
		buff[0]=0;
		f_write(&Fil,buff, 1, &bw);
		int i,j;
		for(i=0;i<8;i++){
			
			uint32_t* copier = &confs[i];
			for (j=0;j<sizeof(conf)/4;j++){
				buff[j]=copier[j];
			}
			f_write(&Fil,buff, sizeof(conf)/4, &bw);
		}
		f_close(&Fil);
		}else{
		while(1);
	}
	
	
}

void load_times(){
	//carefully, this shit accept conf only with size multiple of 4B
	UINT bw;
	uint32_t buff[ sizeof(conf)/4];
	f_mount(&FatFs, "", 0);
	if (f_open(&Fil, "config.txt", FA_READ | FA_OPEN_EXISTING) == FR_OK) {
		f_read(&Fil,buff, 1, &bw);
		active0=buff[0];
		int i,j;
		for(i=0;i<8;i++){
			f_read(&Fil,buff, sizeof(conf)/4, &bw);
			uint32_t* copier = &confs[i];
			for (j=0;j<sizeof(conf)/4;j++){
				copier[j]=buff[j];
			}
		}
		f_close(&Fil);
		}else{
		while(1);
	}
}

#define len(p) confs[p].len
#define times(p) confs[p].times
#define overtime(p) confs[p].overtime
#define undertime(p) confs[p].undertime

typedef struct control_t{
	uint8_t active_conf;

}control;

control controls[2];

static void controller_routine(void *x)
{
	
	
	uint8_t conf_set=((uint8_t)x) & 0x07;
	uint8_t index=((uint8_t)x>>4) & 0x01;
	while(1){
		/*
		wait for pressure
		*/
		uint32_t time_left = 0;
		uint8_t i=0;
		while (i<len(conf_set)){
			time_left=times(conf_set)[i];
			if (overtime(conf_set)>0){
				open_over(index);
				if (overtime(conf_set) < time_left){
					os_sleep(overtime(conf_set));
					time_left= time_left-overtime(conf_set);
				}
				else{
					os_sleep(time_left);
					time_left = 0;
				}
				close_over(index);
			}
			
			if (time_left > 0){
				open_normal(index);
				if (undertime(conf_set)<0xFFFFFFFF){
					if (time_left > undertime(conf_set)){
						os_sleep(undertime(conf_set));
						close_normal(index);
						open_under(index);
						os_sleep(time_left- undertime(conf_set));
						close_under(index);
					}
					else {
						os_sleep(time_left);
						close_normal(index);
					}
				}
				else{
					os_sleep(time_left);
					close_normal(index);
				}
			}
			i++;
			if (i<len(conf_set)){
				os_sleep(times(conf_set)[i]);
				i++;
			}
		}
	}
}


int main(void)
{


	atmel_start_init();
	preparesd();
	load_times();
	
	close_under(0);
	close_under(1);
	
	
	if (xTaskCreate(controller_routine, "C0", TASK_EXAMPLE_STACK_SIZE, (void*)(0 | 4),
	TASK_EXAMPLE_STACK_PRIORITY, xCreatedExampleTask)!= pdPASS) {
		while (1) {
			;
		}
	}

	if (xTaskCreate(controller_routine, "C1", TASK_EXAMPLE_STACK_SIZE, (void*)(1<<4 | 6),
	TASK_EXAMPLE_STACK_PRIORITY, xCreatedExampleTask)!= pdPASS) {
		while (1) {
			;
		}
	}


	vTaskStartScheduler();
	
	while (1) {
	}
}
