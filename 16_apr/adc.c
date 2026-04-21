#include <stdint.h>

#define PERIPH_BASE       0x40000000UL
#define AHB1PERIPH_BASE   (PERIPH_BASE + 0x00020000UL)
#define APB2PERIPH_BASE   (PERIPH_BASE + 0x00010000UL)

#define GPIOA_BASE        (AHB1PERIPH_BASE + 0x0000UL)
#define RCC_BASE          (AHB1PERIPH_BASE + 0x3800UL)
#define ADC1_BASE         (APB2PERIPH_BASE + 0x2000UL)

#define RCC_AHB1ENR       (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR       (*(volatile uint32_t *)(RCC_BASE + 0x44))
#define RCC_CFGR          (*(volatile uint32_t *)(RCC_BASE + 0x08))

#define GPIOA_MODER       (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_PUPDR       (*(volatile uint32_t *)(GPIOA_BASE + 0x0C))

#define ADC1_SR           (*(volatile uint32_t *)(ADC1_BASE + 0x00))
#define ADC1_CR1          (*(volatile uint32_t *)(ADC1_BASE + 0x04))
#define ADC1_CR2          (*(volatile uint32_t *)(ADC1_BASE + 0x08))
#define ADC1_SMPR2        (*(volatile uint32_t *)(ADC1_BASE + 0x10))
#define ADC1_SQR3         (*(volatile uint32_t *)(ADC1_BASE + 0x34))
#define ADC1_DR           (*(volatile uint32_t *)(ADC1_BASE + 0x4C))


void adc_init(void)
{
    RCC_AHB1ENR |= (1 << 0);   
    RCC_APB2ENR |= (1 << 8);  

    RCC_CFGR &= ~(3 << 16);
    RCC_CFGR |=  (2 << 16);

    GPIOA_MODER &= ~(3 << 0);
    GPIOA_MODER |=  (3 << 0);
    GPIOA_PUPDR &= ~(3 << 0);

    ADC1_CR1 = 0;            
    ADC1_CR2 = 0;             

    ADC1_SMPR2 |= (7 << 0);   

    ADC1_SQR3 = 0;

    ADC1_CR2 |= (1 << 0);   

    for (volatile int i = 0; i < 1000; i++);
}

uint16_t adc_read(void)
{
    ADC1_CR2 |= (1 << 30);     

    while (!(ADC1_SR & (1 << 1)));  

    return (uint16_t)ADC1_DR;
}

int main(void)
{
    uint16_t adc_value;

    adc_init();

    while (1)
    {
        adc_value = adc_read();

    }
}