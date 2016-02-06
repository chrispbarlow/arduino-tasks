// Do not remove the include below
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "libTTduino.h"
#include "Arduino.h"

/* This is needed to give the timer ISR access to the correct instance of the schedule */
TTduino* thisSchedule = NULL;

TTduino::TTduino(uint16_t numTasks){
	if(thisSchedule == NULL){
		_tasksUsed = 0;
		_numTasks = numTasks;
		_taskList = new tasks[numTasks];
		_schedLock = true;
		thisSchedule = this;
	}
	else{
		/* Only one instance of the scheduler is allowed */
	}
}

/* Call in setup() Adds a task to the task list */
void TTduino::addTask(task_function_t init, task_function_t update, uint32_t period, uint32_t offset){
	if(_tasksUsed < _numTasks){
		_taskList[_tasksUsed].task_initFunction = init;
		_taskList[_tasksUsed].task_function = update;
		_taskList[_tasksUsed].task_period = period;
		_taskList[_tasksUsed].task_delay = offset;
		_tasksUsed++;
	}
}

/*
 * To be called at the end of setup()
 * Initialises the tasks via their _init functions before starting the timer
 */
void TTduino::begin(uint16_t ticklength){
	int i;

	wdt_disable();			/* Disable the watchdog timer */
	for(i = 0; i < _tasksUsed; i++){
		(*_taskList[i].task_initFunction)();
	}

	tick_Start(ticklength);
}

/* Call as the only method in loop(). Handles scheduling of the tasks */
void TTduino::runTasks(void){
	int i;
	/* Go to sleep. Woken by ISR loop continues, then sleep repeats */
	sleepNow();
/******************** Nothing happens until interrupt tick *****************************************/

	/*_schedLock prevents scheduler from running on non-timer interrupt */
	if (_schedLock == false){
		for(i = 0; i < _tasksUsed; i++){
			/* Task is ready when task_delay = 0 */
			if(_taskList[i].task_delay == 0){
				/* Reload task_delay */
				_taskList[i].task_delay = (_taskList[i].task_period - 1);
				/* Call task function */
				(*_taskList[i].task_function)();
			}
		}
	}
}

/* Start the timer interrupt */
void TTduino::tick_Start(uint16_t period){
	/* initialize Timer1 */
	cli(); 			/* disable global interrupts */
	TCCR1A = 0; 	/* set entire TCCR1A register to 0 */
	TCCR1B = 0; 	/* same for TCCR1B */

	/* set compare match register to desired timer count: */
	OCR1A = (16 * period); /* TICK_PERIOD is in microseconds */
	/* turn on CTC mode: */
	TCCR1B |= (1 << WGM12);
	/* enable timer compare interrupt: */
	TIMSK1 |= (1 << OCIE1A);
	TCCR1B |= (1 << CS10);
	sei(); /* enable global interrupts (start the timer)*/
}

void TTduino::sleepNow(){
	set_sleep_mode(SLEEP_MODE_IDLE);  	/* sleep mode is set here */
	sleep_enable();											/* enables the sleep bit in the mcucr register */
																			/* so sleep is possible. just a safety pin */
	power_adc_disable();								/* Power down some things to save power */
	power_spi_disable();
	power_timer0_disable();
	power_twi_disable();

	_schedLock = true;									/* Prevent schedule from running on accidental wake-up */
	sleep_mode();            						/* here the device is actually put to sleep!! */
/* THE PROGRAM IS WOKEN BY TIMER1 ISR */
}

void __isrTick(){
	int i;
	sleep_disable();        /* disable sleep */
	power_all_enable();			/* restore all power */
	for(i = 0; i < thisSchedule->_tasksUsed; i++){
		/* task delay decremented until it reaches zero (time to run) */
		if(thisSchedule->_taskList[i].task_delay > 0){
			thisSchedule->_taskList[i].task_delay--;
		}
	}
	thisSchedule->_schedLock = false;		/* allow scheduler to run */
}

/* The ISR runs periodically every tick */
ISR(TIMER1_COMPA_vect){
	__isrTick();
}

/* placeholder for tasks that have no initialisation code */
volatile void no_init(){
}