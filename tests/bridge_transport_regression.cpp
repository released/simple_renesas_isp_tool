#include <iostream>
extern "C" {
int TestBridgeI2c(void);
int TestF24I2cBackend(void);
int TestF24Uart(void);
}
int TestMultiBus();
int main() {
    const int failures=TestMultiBus()+(!TestBridgeI2c())+TestF24I2cBackend()+TestF24Uart();
    if(!failures) std::cout<<"PASS unchanged CAN catalog/I2C/G16 and Host Link UART0 regression\n";
    return failures ? 1:0;
}
