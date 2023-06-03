/*
 * Copyright (c) 2023, Nico Maas
 *
 * Change Logs:
 * Date         Author        Notes
 * 2023-06-03   Nico Maas     first version
 */

#include <rtthread.h>
#include <lwip/api.h>
#include <lwip/sockets.h>
#include <lwip/netif.h>
#include <lwip/netifapi.h>

// define SERVER_IP_ADDR, SERVER_NETMASK and SERVER_GATEWAY_IP to manually set IP address
// or leave commented out to activate DHCP Client
//#define SERVER_IP_ADDR      "192.168.178.2"
//#define SERVER_NETMASK      "255.255.255.0"
//#define SERVER_GATEWAY_IP   "192.168.178.1"
#define SERVER_UDP_PORT     4711
#define DEST_IP_ADDR        "192.168.178.3"
#define DEST_UDP_PORT       4712

// CCSDS configuration
#define PACKET_VERSION_NUMBER   0   // Default: 0
#define PACKET_TYPE             0   // Telemetry Packet: 0
// all allowed APID numbers [0-2047] including if the packet is going to have a 2nd header (1) or not (0)
static const unsigned int APID_CONFIG[][2] = {{123, 1}, {815, 1}, {2047, 0}};

#define INCOMING_QUEUE_SIZE 128
#define OUTGOING_QUEUE_SIZE 512

// generated Syncwords
unsigned short SYNCWORDS[(sizeof(APID_CONFIG)/(2*sizeof(unsigned int)))];

static struct udp_pcb* udp_server_pcb;
static rt_mq_t incoming_message_queue;
static rt_mq_t outgoing_message_queue;

static void udp_receive_callback(void* arg, struct udp_pcb* pcb, struct pbuf* p, const ip_addr_t* addr, u16_t port)
{
    // If new UDP packet is received
    if (p != NULL)
    {
        // ... and its not empty
        char* payload = rt_malloc(p->tot_len);
        if (payload != NULL)
        {
            rt_memcpy(payload, p->payload, p->tot_len);
            // split the data to byte size
            for (int i = 0; i < p->tot_len; i++)
            {
                rt_uint8_t bytePart = payload[i];
                // add the bytes of the packet to the message queue
                rt_mq_send(incoming_message_queue, &bytePart, sizeof(rt_uint8_t));
            }
        }
        pbuf_free(p);
        free(payload);
    }
}

static void udp_server_thread(void* parameter)
{
    // Create a UDP server on the current IP address of the network interface
    udp_server_pcb = udp_new();
    udp_bind(udp_server_pcb, IP_ADDR_ANY, SERVER_UDP_PORT);
    udp_recv(udp_server_pcb, udp_receive_callback, NULL);
    rt_kprintf("UDP server started\n");
    while (1)
    {
        rt_thread_mdelay(1000);  // Wait for 1 second
    }
}

// CRC16-CCITT-False Checksum
unsigned short crc16sum(rt_uint8_t *data, unsigned int len, unsigned short crc)
{
    static const unsigned short table[256] = {
    0x0000U,0x1021U,0x2042U,0x3063U,0x4084U,0x50A5U,0x60C6U,0x70E7U,
    0x8108U,0x9129U,0xA14AU,0xB16BU,0xC18CU,0xD1ADU,0xE1CEU,0xF1EFU,
    0x1231U,0x0210U,0x3273U,0x2252U,0x52B5U,0x4294U,0x72F7U,0x62D6U,
    0x9339U,0x8318U,0xB37BU,0xA35AU,0xD3BDU,0xC39CU,0xF3FFU,0xE3DEU,
    0x2462U,0x3443U,0x0420U,0x1401U,0x64E6U,0x74C7U,0x44A4U,0x5485U,
    0xA56AU,0xB54BU,0x8528U,0x9509U,0xE5EEU,0xF5CFU,0xC5ACU,0xD58DU,
    0x3653U,0x2672U,0x1611U,0x0630U,0x76D7U,0x66F6U,0x5695U,0x46B4U,
    0xB75BU,0xA77AU,0x9719U,0x8738U,0xF7DFU,0xE7FEU,0xD79DU,0xC7BCU,
    0x48C4U,0x58E5U,0x6886U,0x78A7U,0x0840U,0x1861U,0x2802U,0x3823U,
    0xC9CCU,0xD9EDU,0xE98EU,0xF9AFU,0x8948U,0x9969U,0xA90AU,0xB92BU,
    0x5AF5U,0x4AD4U,0x7AB7U,0x6A96U,0x1A71U,0x0A50U,0x3A33U,0x2A12U,
    0xDBFDU,0xCBDCU,0xFBBFU,0xEB9EU,0x9B79U,0x8B58U,0xBB3BU,0xAB1AU,
    0x6CA6U,0x7C87U,0x4CE4U,0x5CC5U,0x2C22U,0x3C03U,0x0C60U,0x1C41U,
    0xEDAEU,0xFD8FU,0xCDECU,0xDDCDU,0xAD2AU,0xBD0BU,0x8D68U,0x9D49U,
    0x7E97U,0x6EB6U,0x5ED5U,0x4EF4U,0x3E13U,0x2E32U,0x1E51U,0x0E70U,
    0xFF9FU,0xEFBEU,0xDFDDU,0xCFFCU,0xBF1BU,0xAF3AU,0x9F59U,0x8F78U,
    0x9188U,0x81A9U,0xB1CAU,0xA1EBU,0xD10CU,0xC12DU,0xF14EU,0xE16FU,
    0x1080U,0x00A1U,0x30C2U,0x20E3U,0x5004U,0x4025U,0x7046U,0x6067U,
    0x83B9U,0x9398U,0xA3FBU,0xB3DAU,0xC33DU,0xD31CU,0xE37FU,0xF35EU,
    0x02B1U,0x1290U,0x22F3U,0x32D2U,0x4235U,0x5214U,0x6277U,0x7256U,
    0xB5EAU,0xA5CBU,0x95A8U,0x8589U,0xF56EU,0xE54FU,0xD52CU,0xC50DU,
    0x34E2U,0x24C3U,0x14A0U,0x0481U,0x7466U,0x6447U,0x5424U,0x4405U,
    0xA7DBU,0xB7FAU,0x8799U,0x97B8U,0xE75FU,0xF77EU,0xC71DU,0xD73CU,
    0x26D3U,0x36F2U,0x0691U,0x16B0U,0x6657U,0x7676U,0x4615U,0x5634U,
    0xD94CU,0xC96DU,0xF90EU,0xE92FU,0x99C8U,0x89E9U,0xB98AU,0xA9ABU,
    0x5844U,0x4865U,0x7806U,0x6827U,0x18C0U,0x08E1U,0x3882U,0x28A3U,
    0xCB7DU,0xDB5CU,0xEB3FU,0xFB1EU,0x8BF9U,0x9BD8U,0xABBBU,0xBB9AU,
    0x4A75U,0x5A54U,0x6A37U,0x7A16U,0x0AF1U,0x1AD0U,0x2AB3U,0x3A92U,
    0xFD2EU,0xED0FU,0xDD6CU,0xCD4DU,0xBDAAU,0xAD8BU,0x9DE8U,0x8DC9U,
    0x7C26U,0x6C07U,0x5C64U,0x4C45U,0x3CA2U,0x2C83U,0x1CE0U,0x0CC1U,
    0xEF1FU,0xFF3EU,0xCF5DU,0xDF7CU,0xAF9BU,0xBFBAU,0x8FD9U,0x9FF8U,
    0x6E17U,0x7E36U,0x4E55U,0x5E74U,0x2E93U,0x3EB2U,0x0ED1U,0x1EF0U,
    };

    while (len > 0)
    {
        crc = table[*data ^ (unsigned char)(crc >> 8)] ^ (crc << 8);
        data++;
        len--;
    }
    return crc;
}

static void state_machine_thread(void* parameter)
{
    //rt_kprintf("SYNCWORDS:\n");
    for (int i = 0; i < (sizeof(APID_CONFIG)/(2*sizeof(unsigned int))); i++) {
        SYNCWORDS[i] |= (PACKET_VERSION_NUMBER & 0x07)  << 13;
        SYNCWORDS[i] |= (PACKET_TYPE & 0x01) << 12;
        SYNCWORDS[i] |= (APID_CONFIG[i][1] & 0x01) << 11;
        SYNCWORDS[i] |= (APID_CONFIG[i][0] & 0x7FF);
        //rt_kprintf("- 0x%04hX\n", (unsigned short)SYNCWORDS[i]);
    }
    // buffers
    rt_uint8_t fsm_init = 0;
    rt_uint8_t fsm_mode = 0;
    rt_uint8_t packet_length = 0;
    rt_uint8_t tmp_data;
    rt_uint8_t tmp_data2;
    rt_kprintf("Finite State machine started\n");
    while (1)
    {
        // try to find the start of the CCSDS TM packet (SYNCWORDS)
        if (fsm_mode==0)
        {
            // have not read any data, read two bytes and compare if it matches any SYNCWORDS
            if (fsm_init==0)
            {
                tmp_data = 0;
                tmp_data2 = 0;
                //rt_kprintf(" output queue: %d \n", fsm_queue->entry);
                // try to get first part of the sync word
                if (rt_mq_recv(incoming_message_queue, &tmp_data, sizeof(rt_uint8_t), RT_WAITING_FOREVER) == RT_EOK)
                {
                    // try to get second byte of the syncword
                    if (rt_mq_recv(incoming_message_queue, &tmp_data2, sizeof(rt_uint8_t), RT_WAITING_FOREVER) == RT_EOK)
                    {
                        // look for configured syncword in APID_CONFIG
                        for (int i = 0; i < (sizeof(APID_CONFIG)/(2*sizeof(unsigned int))); i++)
                        {
                            // check if read 2 bytes match any SYNCWORDS
                            if  ( (tmp_data << 8 | tmp_data2) == SYNCWORDS[i] )
                            {
                                // yes, add to queue, advance to mode 2, reset buffers
                                rt_mq_send(outgoing_message_queue, &tmp_data, sizeof(rt_uint8_t));
                                rt_mq_send(outgoing_message_queue, &tmp_data2, sizeof(rt_uint8_t));
                                fsm_mode=2;
                                tmp_data=0;
                                tmp_data2=0;
                                fsm_init=0;
                                rt_kprintf(" ok: mode 0->2\n");
                                //rt_kprintf(" output queue: %d \n", fsm_queue->entry);
                                break;
                            }
                            else
                            {
                                // no, stay in mode 0 but change fsm_init to 1
                                // this means two bytes have already been read
                                // from now on, the first byte will be dropped and replaced by the second one
                                // second position will be filled with a freshly read byte to make sure every combination
                                // is tested
                                fsm_mode = 0;
                                fsm_init = 1;
                                rt_kprintf(" decode error, init 0: mode 0->0\n");
                            }
                        }
                    }
                }
            }
            else
            {
                // fsm_init==1 means two bytes have already been read and do not match any SYNCWORDS
                // the first byte will be replaced by the second byte and a new byte will be read for the second byte
                // then compare starts again
                tmp_data = tmp_data2;
                tmp_data2 = 0;
                //rt_kprintf(" output queue: %d \n", fsm_queue->entry);
                // try to get second byte of the syncword
                if (rt_mq_recv(incoming_message_queue, &tmp_data2, sizeof(rt_uint8_t), RT_WAITING_FOREVER) == RT_EOK)
                {
                    // look for configured syncword in APID_CONFIG
                    for (int i = 0; i < (sizeof(APID_CONFIG)/(2*sizeof(unsigned int))); i++)
                    {
                        // check if read 2 bytes match any SYNCWORDS
                        if  ( (tmp_data << 8 | tmp_data2) == SYNCWORDS[i] )
                        {
                            // yes, add to queue, advance to mode 2, reset buffers
                            rt_mq_send(outgoing_message_queue, &tmp_data, sizeof(rt_uint8_t));
                            rt_mq_send(outgoing_message_queue, &tmp_data2, sizeof(rt_uint8_t));
                            fsm_mode=2;
                            tmp_data=0;
                            tmp_data2=0;
                            fsm_init=0;
                            rt_kprintf(" ok: mode 0->2\n");
                            //rt_kprintf(" output queue: %d \n", fsm_queue->entry);
                            break;
                        }
                        else
                        {
                            // this did not match, stay in mode 0, init 1 and keep on shifting position 2 to position 1,
                            // and place a newly read byte to position 2
                            fsm_mode = 0;
                            fsm_init = 1;
                            rt_kprintf(" decode error, init 1: mode 0->0\n");
                        }
                    }
                }
            }
        }
        // mode 2, SYNCWORD was found, now read sequence flag and sequence count
        else if (fsm_mode==2)
        {
            //rt_kprintf(" start: mode 2\n");
            // read sequence flag (2 bit), sequence count (14 bit)
            if (rt_mq_recv(incoming_message_queue, &tmp_data, sizeof(rt_uint8_t), RT_WAITING_FOREVER) == RT_EOK)
            {
                //rt_kprintf(" start: mode 2a\n");
                if (rt_mq_recv(incoming_message_queue, &tmp_data2, sizeof(rt_uint8_t), RT_WAITING_FOREVER) == RT_EOK)
                {
                    //rt_kprintf(" start: mode 2b\n");
                    rt_mq_send(outgoing_message_queue, &tmp_data, sizeof(rt_uint8_t));
                    rt_mq_send(outgoing_message_queue, &tmp_data2, sizeof(rt_uint8_t));
                    tmp_data=0;
                    tmp_data2=0;
                    fsm_mode = 21;
                    rt_kprintf(" ok: mode 2->21\n");
                    //rt_kprintf(" output queue: %d \n", fsm_queue->entry);
                }
            }
        }
        // mode 21, sequence flag and sequence count were read, read data length field
        // calculate the length of the data portion without the CRC fields
        else if (fsm_mode==21)
        {
            //rt_kprintf(" start: mode 21\n");
            // ... and data length (16 bit) fields
            if (rt_mq_recv(incoming_message_queue, &tmp_data, sizeof(rt_uint8_t), RT_WAITING_FOREVER) == RT_EOK)
            {
                if (rt_mq_recv(incoming_message_queue, &tmp_data2, sizeof(rt_uint8_t), RT_WAITING_FOREVER) == RT_EOK)
                {
                    rt_mq_send(outgoing_message_queue, &tmp_data, sizeof(rt_uint8_t));
                    rt_mq_send(outgoing_message_queue, &tmp_data2, sizeof(rt_uint8_t));
                    //rt_kprintf(" length: %d \n", (tmp_data << 8 | tmp_data2) );
                    packet_length = (tmp_data << 8 | tmp_data2) - 1;
                    // data length, + 2 missing for CRC
                    tmp_data=0;
                    tmp_data2=0;
                    fsm_mode = 22;
                    rt_kprintf(" ok: mode 21->22 with packet_length: %d\n", packet_length);
                    //rt_kprintf(" output queue: %d \n", fsm_queue->entry);
                }
            }
        }
        // mode 22, data length field has been read,
        // start reading the payload data
        else if (fsm_mode==22)
        {
            //rt_kprintf(" mode 22 reached - data load\n");
            //rt_kprintf(" pre output queue: %d \n", fsm_queue->entry);
            for (int i = 0; i < packet_length; i++)
            {
                if (rt_mq_recv(incoming_message_queue, &tmp_data, sizeof(rt_uint8_t), RT_WAITING_FOREVER) == RT_EOK)
                {
                    //rt_kprintf(" load byte %d of %d\n", i, packet_length);
                    rt_mq_send(outgoing_message_queue, &tmp_data, sizeof(rt_uint8_t));
                    tmp_data=0;
                }
            }
            //rt_kprintf(" after output queue: %d \n", fsm_queue->entry);
            fsm_mode=23;
            rt_kprintf(" ok: mode 22->23 with %d bytes payload loaded\n", packet_length);
        }
        // mode 23, payload data has been read,
        // read the CRC fields,
        // calculate the real CRC value and compare with the sent ones
        // if ok, forward data via UDP
        // if not, reset and start again from mode 0
        else if (fsm_mode==23)
        {
            //rt_kprintf(" mode 23 reached - crc\n");
            if (rt_mq_recv(incoming_message_queue, &tmp_data, sizeof(rt_uint8_t), RT_WAITING_FOREVER) == RT_EOK)
            {
                if (rt_mq_recv(incoming_message_queue, &tmp_data2, sizeof(rt_uint8_t), RT_WAITING_FOREVER) == RT_EOK)
                {
                    // insert CRC data
                    rt_mq_send(outgoing_message_queue, &tmp_data, sizeof(rt_uint8_t));
                    rt_mq_send(outgoing_message_queue, &tmp_data2, sizeof(rt_uint8_t));
                    // read how much data exists in the queue
                    rt_uint8_t output_queue_length = outgoing_message_queue->entry;
                    //rt_kprintf(" entry output queue: %d \n",output_queue_length);
                    // if queue is longer than 0 byte
                    if (output_queue_length > 0)
                    {
                        // Transfer data from the output queue to a byte array
                        rt_uint8_t byte_array[output_queue_length];
                        for (int i = 0; i < output_queue_length; i++)
                        {
                            rt_mq_recv(outgoing_message_queue, &byte_array[i], sizeof(rt_uint8_t), RT_WAITING_NO);
                        }
                        //rt_kprintf(" size of byte array: %d \n", sizeof(byte_array));
                        // calculate real CRC value of received data
                        rt_uint16_t chkSum = crc16sum(byte_array, output_queue_length-2, 0xFFFF);
                        //rt_kprintf(" computed crc: %d \n", chkSum);
                        // show CRC value sent with message
                        //rt_kprintf(" sent crc: %d \n", tmp_data << 8 | tmp_data2);
                        // compare if they are identical / message was transfered correctly
                        if ((tmp_data << 8 | tmp_data2) == chkSum) {
                            rt_kprintf(" chksum correct (%d), sending data \n", chkSum);
                            // Send the content via UDP to the preconfigured IP and port
                            struct udp_pcb* udp_client_pcb = udp_new();
                            ip4_addr_t dest_ip;
                            ipaddr_aton(DEST_IP_ADDR, &dest_ip);
                            struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, sizeof(byte_array), PBUF_RAM);
                            if (p != NULL)
                            {
                                rt_memcpy(p->payload, byte_array, sizeof(byte_array));
                                udp_sendto(udp_client_pcb, p, &dest_ip, DEST_UDP_PORT);
                                pbuf_free(p);
                            }
                            //rt_free(byte_array);
                            udp_remove(udp_client_pcb);
                        }
                        else
                        {
                            rt_kprintf(" chksum error (sent: %d, computed: %d), restarting \n", (tmp_data << 8 | tmp_data2), chkSum);
                        }
                    }
                    // delete queue content
                    while (rt_mq_recv(outgoing_message_queue, &tmp_data, sizeof(rt_uint8_t), 1) == RT_EOK) {
                        tmp_data=0;
                    }
                    // reset buffers
                    tmp_data=0;
                    tmp_data2=0;
                    fsm_mode=0;
                    fsm_init=0;
                    rt_kprintf(" ok: mode 23->0\n");
                } // crc2
            } // crc1
        } // mode 23
    } // while
}

int app_thread_create(void)
{
    rt_kprintf("CCSDS-TM-FSM\n");
    rt_kprintf("Nico Maas, 2023\n");
    rt_kprintf("www.nico-maas.de\n");
    rt_kprintf("Packet Version: %d\n", PACKET_VERSION_NUMBER);
    rt_kprintf("Packet Type: %d\n", PACKET_TYPE);
    rt_kprintf("Packet APID/Packet has 2nd Header:\n");
    for (int i = 0; i < (sizeof(APID_CONFIG)/(2*sizeof(unsigned int))); i++) {
        rt_kprintf("- %d/%d\n", APID_CONFIG[i][0], APID_CONFIG[i][1]);
    }

    // Get network interface
    struct netif* netif = netif_default;
    // Set manual IP addr or do auto configuration via DHCP
    #ifdef SERVER_IP_ADDR
    dhcp_stop(netif);
    ip4_addr_t server_ip;
    ip4_addr_t server_netmask;
    ip4_addr_t server_gateway_ip;
    ipaddr_aton(SERVER_IP_ADDR, &server_ip);
    ipaddr_aton(SERVER_NETMASK, &server_netmask);
    ipaddr_aton(SERVER_GATEWAY_IP, &server_gateway_ip);
    netif_set_addr(netif, &server_ip, &server_netmask, &server_gateway_ip );
    rt_kprintf("Setting IP manually");
    #else
    rt_kprintf("Setting IP via DHCP");
    #endif
    // wait for network interface to have IP
    while (1) {
        if (netif != RT_NULL && netif_is_up(netif) && ip4_addr_get_u32(netif_ip4_addr(netif)) != 0) {
            break;
        }
        rt_kprintf(".");
        rt_thread_delay(RT_TICK_PER_SECOND);
    }
    rt_kprintf("\n");
    rt_kprintf("UDP Server: %s, UDP Port: %d\n", ip4addr_ntoa(netif_ip4_addr(netif)), SERVER_UDP_PORT);
    rt_kprintf("Target System: %s, UDP Port: %d\n", DEST_IP_ADDR, DEST_UDP_PORT);

    // Create the incoming message queue
    incoming_message_queue = rt_mq_create("incoming_message_queue", sizeof(rt_uint8_t), INCOMING_QUEUE_SIZE, RT_IPC_FLAG_FIFO);
    if (incoming_message_queue == RT_NULL)
    {
        rt_kprintf("Failed to create incoming message queue\n");
        return -1;
    }

    // Create the UDP server thread
    rt_thread_t udp_server_tid = rt_thread_create("udp_server",
                                                  udp_server_thread,
                                                  RT_NULL,
                                                  2048,
                                                  8,
                                                  10);
    if (udp_server_tid != RT_NULL)
    {
        rt_thread_startup(udp_server_tid);
    }
    else
    {
        rt_kprintf("Failed to create UDP server thread\n");
        return -1;
    }

    // Create the outgoing message queue
    outgoing_message_queue = rt_mq_create("outgoing_message_queue", sizeof(rt_uint8_t), OUTGOING_QUEUE_SIZE, RT_IPC_FLAG_FIFO);
    if (outgoing_message_queue == RT_NULL)
    {
        rt_kprintf("Failed to create outgoing message queue\n");
        return -1;
    }

    // Create the state machine thread
    rt_thread_t state_machine_tid = rt_thread_create("state_machine",
                                                    state_machine_thread,
                                                    RT_NULL,
                                                    2048,
                                                    6,
                                                    10);
    if (state_machine_tid != RT_NULL)
    {
        rt_thread_startup(state_machine_tid);
    }
    else
    {
        rt_kprintf("Failed to create state machine thread\n");
        return -1;
    }

    return 0;
}

// start application
INIT_APP_EXPORT(app_thread_create);
