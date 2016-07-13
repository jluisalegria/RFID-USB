#include <18F2455.h>
#device ADC = 10
#fuses HSPLL,NOWDT,NOPROTECT,NOLVP,NODEBUG,USBDIV,PLL5,CPUDIV1,VREGEN,PUT
#use delay(clock=48000000)
#use rs232(STREAM=debug,  baud=38400, xmit =pin_B7, rcv = pin_B6)
#use fast_io(A)
#use fast_io(b)
#use fast_io(c)

//#DEFINE  USB_HID_DEVICE  TRUE

/*#define USB_EP1_TX_ENABLE  USB_ENABLE_INTERRUPT   //turn on EP1 for IN bulk/interrupt transfers
#define USB_EP1_TX_SIZE 8

#define USB_EP1_RX_ENABLE  USB_ENABLE_INTERRUPT   //turn on EP1 for IN bulk/interrupt transfers
#define USB_EP1_RX_SIZE 8*/
#include <pic18_usb.h>   //Microchip PIC18Fxx5x hardware layer for usb.c
//#include <usbn960x.h>   //National 960x hardware layer for usb.c
#include "usb_desc_FI.h"    //USB Configuration and Device descriptors for this UBS device
#include <usb.c>        //handles usb setup tokens and get descriptor reports
#include <ctype.h>

int8 connected;
int8 enumerated;
int8 rx_msg[USB_EP1_RX_SIZE];
unsigned int8 tx_msg[7]={0,0,0,0,0,0,0};     //tx_msg[2] = dato a enviar
int8 Caracter[8] = {0};
//****************************************************************************
//******************Variables para recepcion wiegand**************************
int8 interrupt0;
int8 interrupt2;
int1 ReceivingCodeWiegand;
int1 CodeWiegandReceived;
int8 Timer1Interrupts;
int8 WiegandBits[30];
int8 SerialCode[6];
int8 IndexBits;
int8 i;
int8 ascci;
int16 Blink=0;

int8 bits[8]={128,64,32,16,8,4,2,1};
int8 ID[10];
int16 ID_debug=0;
//******************************************************************************

int8 char_2_usb_kbd_code(char c){

   int8 ic;

   if(isAlpha(c)){
      ic=c-'a'+4;
   }
   else{
      if(c=='0'){
        ic=39;
      }
      else{
        ic=c-'1'+30;
      }
   }
   return(ic);
}

//----------------------------------------------------------------------------//
/*  interrupcion timer1*/
#int_Timer1
void int_timer1_handler(void)
{
 ++Timer1Interrupts;
 if(Timer1Interrupts >= 5)
 {
  CodeWiegandReceived = 1;
  disable_interrupts(int_Timer1);
  //disable_interrupts(global);
 }
 
}
//----------------------------------------------------------------------------//

/* interrupcion INT0  DATA 0*/   
#int_ext
void int_ext_handler(void)
{
 ++interrupt0;
 WiegandBits[IndexBits++] = 0;
 if(ReceivingCodeWiegand)
 {
  set_timer1(0);
 }
 else
 {
  ReceivingCodeWiegand = 1;
  setup_timer_1( T1_INTERNAL | T1_DIV_BY_8 );
  set_timer1(0);
  enable_interrupts(int_Timer1);
 }
}
//----------------------------------------------------------------------------//
/* interrupcion INT1  DATA 1*/
#int_ext2
void int_ext1_handler(void)
{
 WiegandBits[IndexBits++] = 1;
 ++interrupt2;
 if(ReceivingCodeWiegand)
 {
  set_timer1(0);
 }
 else
 {
  ReceivingCodeWiegand = 1;
  setup_timer_1 ( T1_INTERNAL | T1_DIV_BY_8 );
  set_timer1(0);
  enable_interrupts(int_Timer1);
 }
}

//----------------------------------------------------------------------------//
void usb_rx_task(void){

   if (usb_kbhit(1)){
      usb_get_packet(1, rx_msg, sizeof(rx_msg));
   }
}
//------------------------------------------------------------------------------
void RestartRx(void)
{
 interrupt0 = 0;
 interrupt2 = 0;
 Timer1Interrupts = 0;
 
 ReceivingCodeWiegand = 0;
 CodeWiegandReceived = 0;
 IndexBits = 1;
 
 for(i=0;i<26;i++)
 {
  WiegandBits[i] = 0;
 }
 
 clear_interrupt(int_ext);
 clear_interrupt(int_ext2);
 enable_interrupts(global);
}
//----------------------------------------------------------------------------//
void Wiegand34ToSerial(void)
{
      for(int v=0;v<10;v++)
      {
         ID[v] = 0;
      }

// obtencion del facility code
   for(i=2;i<=9;i++)
   {
      if(WiegandBits[i] == 1)
      {
         ID[6] += bits[i-2];
      }
   }
// obtencion del Byte alto del ID
   for(i=10;i<=17;i++)
   {
       if(WiegandBits[i] == 1)
      {
         ID[7] += bits[i-10];
      }
   }
// obtencion del Byte bajo del ID
   for(i=18;i<=25;i++)
   {
      if(WiegandBits[i] == 1)
      {
        ID[8] += bits[i-18];
      }
   }
   
    for(i=26;i<=33;i++)
   {
      if(WiegandBits[i] == 1)
      {
        ID[9] += bits[i-26];
      }
   }
     ID_debug = ((ID[8]*0X100) + ID[9]);
    //fprintf(debug,"Tarjeta leida (L1): %03u,%05Lu\r\n",ID[7],ID_debug);
    fprintf(debug,"Tarjeta leida (L1): %X,%X,%X,%X,\r\n",ID[6],ID[7],ID[8],ID[9]);
   
}
//****************************************************************************
void Convierte_a_USBHID(void)
{

   Caracter[0]= ID[6]>>4 & 0X0F;
   Caracter[1]= ID[6] & 0X0F;
   Caracter[2]= ID[7]>>4 & 0X0F;
   Caracter[3]= ID[7] & 0X0F;
   Caracter[4]= ID[8]>>4 & 0X0F;
   Caracter[5]= ID[8] & 0X0F;
   Caracter[6]= ID[9]>>4 & 0X0F;
   Caracter[7]= ID[9] & 0X0F;
   fprintf(debug,"A convertir: %X,%X,%X,%X,%X,%X,%X,%X,\r\n",Caracter[0],Caracter[1],Caracter[2],Caracter[3],Caracter[4],Caracter[5],Caracter[6],Caracter[7]);
   //fprintf(debug,"ID tarjeta: ");
   
   tx_msg[0]=0;      //indica left shift para mandar mayusculas
   for(int8 j=0;j<8;j++)
   {
   //fprintf(debug,"iteracion %X: ", j);
      tx_msg[0]=0;        //Sin teclas especiales presionadas
      tx_msg[2]=0;     //Tecla enter
      usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
      delay_ms(10);
       usb_task();
      switch (Caracter[j])
      {
         case 0X00:  tx_msg[2]=0X27;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                     usb_task();
                     //fprintf(debug,"0");
                     break;
         case 0X01:  tx_msg[2]=0X1E;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                     usb_task();
                      //fprintf(debug,"1");
                    
                     break;
         case 0X02:  tx_msg[2]=0X1F;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                      usb_task();
                      //fprintf(debug,"2");
                
                     break;
         case 0X03:  tx_msg[2]=0X20;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                      usb_task();
                      //fprintf(debug,"3");
                     
                     break;
         case 0X04:  tx_msg[2]=0X21;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                      usb_task();
                      //fprintf(debug,"4");
                    
                     break;
         case 0X05:  tx_msg[2]=0X22;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                      usb_task();
                      //fprintf(debug,"5");
                     
                     break;
         case 0X06:  tx_msg[2]=0X23;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                     usb_task();
                     // fprintf(debug,"6");
                    
                     break;
         case 0X07:  tx_msg[2]=0X24;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                     usb_task();
                      //fprintf(debug,"7");
                  
                     break;
         case 0X08:  tx_msg[2]=0X25;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                     usb_task();
                      //fprintf(debug,"8");
                   
                     break;
         case 0X09:  tx_msg[2]=0X26;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                     usb_task();
                      //fprintf(debug,"9");
                   
                     break;
         case 0X0A:  tx_msg[2]=0X04;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                      usb_task();
                      //fprintf(debug,"A");
                     
                     break;
         case 0X0B:  tx_msg[2]=0X05;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                     usb_task();
                      //fprintf(debug,"B");
                     
                     break;
         case 0X0C:  tx_msg[2]=0X06;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                      usb_task();
                      //fprintf(debug,"C");
                    
                     break;
         case 0X0D:  tx_msg[2]=0X07;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                      usb_task();
                      //fprintf(debug,"D");
                    
                     break;
         case 0X0E:  tx_msg[2]=0X08;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                      usb_task();
                      //fprintf(debug,"E");
                     
                     break;
         case 0X0F:  tx_msg[2]=0X09;
                     usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
                     delay_ms(10);
                     usb_rx_task();
                     usb_task();
                      //fprintf(debug,"F");
                     
                     break;
         default:    fprintf(debug,"error\r\n");
                     break;
      }
   }
   
    tx_msg[0]=2;        //Sin teclas especiales presionadas
    tx_msg[2]=0X28;     //Tecla enter
    usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
    delay_ms(10);
    usb_rx_task();
   usb_task();
    tx_msg[0]=0;        //Sin teclas especiales presionadas
    tx_msg[2]=0;     //Tecla enter
    usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
    delay_ms(10);
    usb_rx_task();
    usb_task();
    fprintf(debug,"terminado\r\n");
    
}
//****************************************************************************
void receive_code(void)
{
 if(CodeWiegandReceived)
 {
  if(interrupt0 + interrupt2 == 34)
  {
   CodeWiegandReceived = 0;

   Wiegand34ToSerial();
   
   Convierte_a_USBHID();
   
   delay_ms(10);
   
   RestartRx();
  }
  else
  {
   delay_ms(10);
   RestartRx();
  }
 }
}

void main() {

   delay_ms(500);
   fprintf(debug,"inicializando");
   usb_init_cs();

   //port_b_pullups(true);
   
    RestartRx();
    
    ext_int_edge(0,H_TO_L);
    ext_int_edge(2,H_TO_L);
    
    clear_interrupt(int_ext);
    clear_interrupt(int_ext2);
    enable_interrupts(int_ext);
    enable_interrupts(int_ext2);
    enable_interrupts(global);
  
   while (TRUE) {

      usb_task();

      if (usb_enumerated()) {
            receive_code();
         //usb_keyboard_task();
         //usb_rx_task();
         /*tx_msg[2]=0x12;
         usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
         delay_ms(10);
         
         tx_msg[2]=0X1C;
         usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
         delay_ms(10);
         
          tx_msg[2]=0x08;
         usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
         delay_ms(10);
         
         tx_msg[2]=0X2C;
         usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
         delay_ms(10); tx_msg[2]=0x12;
         
          tx_msg[2]=0x06;
         usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
         delay_ms(10);
         
         tx_msg[2]=0X0b;
         usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
         delay_ms(10);
         
          tx_msg[2]=0x18;
         usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
         delay_ms(10);
         
         tx_msg[2]=0X1C;
         usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
         delay_ms(10);
         
          tx_msg[2]=0x28;
         usb_put_packet(1,tx_msg,sizeof(tx_msg),USB_DTS_TOGGLE);
         delay_ms(10);   */
      }
   }
}

