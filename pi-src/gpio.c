/*
 * Name: Eleanor Sigrest
 * Date: 5/13/2024
 * This file is the gpio library. It creates the ability to interact 
 * and manipulate the pins on the Raspberry Pi
 *
 */
#include "gpio.h"

void gpio_init(void) {
    // no initialization required for GPIO peripheral
}

void gpio_set_function(unsigned pin, gpio_func_t function) {
    if (pin >= 0 && pin <= 53 && function >= 0 && function <= 7){
        unsigned int reg = pin/10;
        int index = (pin%10)*3;
        
        unsigned int clearBits =  ~(7 << index);
        
        unsigned int workingFsel = gpio->fsel[reg];
        workingFsel = workingFsel & clearBits; 
        
        unsigned int setBits = function << index;
        workingFsel = workingFsel | setBits;
        
        gpio->fsel[reg] = workingFsel;
    }
}

unsigned int gpio_get_function(unsigned int pin) {
    if (pin < 0 || pin > 53){
        return GPIO_INVALID_REQUEST;
    }
    
    unsigned int reg = pin/10;
    int index = (pin%10)*3;
    unsigned int workingFsel = gpio->fsel[reg];
    unsigned int clearBits = (7 << index);
    workingFsel = (workingFsel & clearBits) >> index;
     
    return workingFsel;
}

void gpio_set_input(unsigned pin) {
    gpio_set_function(pin, GPIO_FUNC_INPUT);
}

void gpio_set_output(unsigned pin) {
    gpio_set_function(pin, GPIO_FUNC_OUTPUT);
}

void gpio_write(unsigned int pin, unsigned int value) {
    if (pin >= 0 && pin <= 53 && value >= 0 && value <= 1){
        unsigned int reg = pin/32;
        int index = pin%32;
    
        if(value == 0){
            gpio->clr[reg] = 1<<index;
        }
        else if(value == 1){
            gpio->set[reg] = 1<<index;
        }
    }
}

unsigned gpio_read(unsigned int pin) {
    if (pin < 0 || pin > 53){
        return GPIO_INVALID_REQUEST;
    }
     
    unsigned int reg = pin/32;
    int index = pin%32;
     
    unsigned int workingLev = gpio->lev[reg];
    
    unsigned int clearBits = (1 << index);
    workingLev = (workingLev & clearBits) >> index;

    return workingLev;
}


void gpio_set_on(unsigned pin) {
    gpio_write(pin, 1);
}

void gpio_set_off(unsigned pin) {
    gpio_write(pin, 0);
}


// Uncomment when implemented.
#if 0
// In process of implementing
void gpio_set_pullup(unsigned pin) {
    gpio->pull[0] = 0b10;
    
    unsigned start_cycle = cycle_cnt_read();
    unsigned end_cycle = cycle_cnt_read();
    while(end_cycle - start_cycle < 150){
        end_cycle = cycle_cnt_read();
    }

    // TO DO
    // Check 1 or 2 based on which bank the GPIO is in
    // Also check shift value based on GPIO bank
    gpio->pull[1] = 1 << pin;
    
    start_cycle = cycle_cnt_read();
    while(end_cycle - start_cycle < 150){
        end_cycle = cycle_cnt_read();
    }

    gpio->pull[0] = 0b00;

    // TO DO
    // Check 1 or 2 based on which bank the GPIO is in
    // Also check shift value based on GPIO bank
    gpio->pull[1] = 1 << pin;

}

// In process of implementing
void gpio_set_pulldown(unsigned pin) {

}


// TO DO: Implement interrupts on GPIO
#endif


