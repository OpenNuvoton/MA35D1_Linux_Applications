# MA35D1-Rpmsg

**Note: MA35D1-RPMSG is a legacy. Please refer to [MA35D1-AMP](https://github.com/OpenNuvoton/MA35D1_Linux_Applications/tree/master/examples/amp) for better performance and stability.**

Nuvoton provides inter-processor communication between the A35 core and RTP core based on the RPMSG framework using OpenAMP.

# OpenAMP
OpenAMP is a third-party open-source software that provides inter-core communication interfaces and frameworks such as REMOTEPROC, RPMSG, and VIRTIO. This following document will explain the architecture and usage of MA35D1 rpmsg.

[OpenAMP](https://github.com/OpenAMP/open-amp?tab=readme-ov-file)

## RPMSG Architecture
RPMSG provides a framework for inter-processor communication and is complemented by IOCTL to make the design more flexible. The architecture uses a single device to manage multiple RPMSG endpoints. 

- **RPMSG**

*Device*
```c
    /dev/rpmsg_ctrl0
```
*Endpoints*
```c
    /dev/rpmsg0
```
- **IOCTL**

*rpmsg_ctrl*
```c
    open()
```
*rpmsg*
```c
    ioctl(create/destroy), open(), write(), poll(), read()
```


# Linux driver
The MA35D1 benefits from hardware support through the Wormhole Controller (WHC), and the MA35D1 rpmsg driver is designed to use WHC as the inter-processor communication interrupt (IPI).

Nuvoton provides two versions of rpmsg, Rpmsg-v1 and Rpmsg-v2, which can be selected through the device tree.

**Note: Rpmsg-v1 is only maintained for existing customers. It is strongly recommended to use Rpmsg-v2.**

## MA35D1-Wormhole driver
[ma35d1-wormhole.c](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/drivers/mailbox/ma35d1-wormhole.c)

- **Based on mailbox driver**

- **WHC hardware features:**
  1. Up to 4 channels WHC#0~3, each channel is bidirectional

  2. Command register: 4 words length data register for each direction

  3. Interrupt: Capable of triggering interrupts to the remote core

## MA35D1-Rpmsg driver
[ma35d1_rpmsg.c](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/drivers/rpmsg/ma35d1_rpmsg.c)

- **Based on rpmsg driver**

- **MA35D1-Rpmsg-v1**

1. Shared memory:

    - Default packet length is 128 bytes in SRAM.

**DTS settings:**
[ma35d1.dtsi](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/arch/arm64/boot/dts/nuvoton/ma35d1.dtsi)
```dts
    rpmsg {
            share-mem-addr = <0x2401ff00>;
            tx-smem-size = <128>;
            rx-smem-size = <128>;
        };
```

**RTP settings:**
[openamp_conf.h](https://github.com/OpenNuvoton/MA35D1_RTP_BSP/blob/master/SampleCode/OpenAMP/rpmsg_rtp/porting/openamp_conf.h)
```c
    #define Share_Memory_Size 0x80*2
```

2. Driver framework

   - The 1st layer handshake is a WHC hardware acknowledgment, which is used to inform the remote side that the data in shared memory has been copied and enqueued into the local Rx buffer.

   - Since the queue of the rpmsg driver doesn't have a strict limit, it may lead to an interrupt storm if the remote side sends packets too quickly, which also prevents the application from having a chance to dequeue the data. Therefore, a 2nd layer was added to handle this issue.

   - The 2nd layer handshake was implemented in rpmsg driver in rpmsg-v1, meaning that an additional command packet is required to complete the handshake, but it did not account for full-duplex operation. This led to the possibility of the WHC layer's data packet and acknowledgment packet overlapping, causing communication errors.

   - To handle the issue mentioned above, one solution is to create a new channel for acknowledgments, while another is to move the acknowledgment to the application layer, allowing both sides' applications to handle packet transmission and reception. Since the number of channels is limited and we plan to expand to support multiple endpoints in the future, the latter approach was chosen for rpmsg-v2.

- **MA35D1-Rpmsg-v2**
1. Shared memory

    - *SRAM* : The default value is 128 bytes. If adjustments are needed, be cautious of potential overlap with the RTP program's text/stack/heap range.

    - *DRAM* : The adjustable range is from 128B to 16KB; the following uses 16KB as an example.

**DTS settings:**
[ma35d1.dtsi](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/arch/arm64/boot/dts/nuvoton/ma35d1.dtsi)

*SRAM*
```dts
    rpmsg {
            share-mem-addr = <0x2401ff00>;
            tx-smem-size = <128>;
            rx-smem-size = <128>;
            rpmsg-v2-arch;
        };
```
*DRAM*
```dts
    reserved-memory {
		rpmsg_buf: rpmsg_buf@0 {
			reg = <0x0 0x80080000 0x0 0x8000>; /* maximun tx+rx 16+16KB */
			no-map;
		};
	};

    rpmsg {
            rpmsg-v2-arch;
            rpmsg-ddr-buf;
            memory-region = <&rpmsg_buf>;
        };
```

**RTP settings:**
[openamp_conf.h](https://github.com/OpenNuvoton/MA35D1_RTP_BSP/blob/master/SampleCode/OpenAMP/rpmsg_rtp/porting/openamp_conf.h)

*SRAM*
```c
    #define Share_Memory_Size 0x80*2
```
*DRAM*
```c
    #define RPMSG_DDR_BUF
    #define RPMSG_V2_ARCH
    #define Share_Memory_Size 0x4000*2
```

2. Driver framework

   - The 1st layer handshake is a WHC hardware acknowledgment, which is used to inform the remote side that the data in shared memory has been copied and enqueued into the local Rx buffer.

    - The 2nd layer handshake has been moved to the application layer (see Applications).



# Applications
Nuvoton provides sample code for A35 and RTP, demonstrating inter-processor communication between the A35 core and RTP core in the MA35D1-Rpmsg architecture.

For the Linux side applications, please choose between Rpmsg-v1 or Rpmsg-v2 based on the driver configuration. For the RTP side application, you can switch between the two versions using preprocessor define **RPMSG_V2_ARCH**
.

**Note: Rpmsg-v1 is only maintained for existing customers. It is strongly recommended to use Rpmsg-v2.**

## Rpmsg-v1
- **Features**
    - Half-duplex

    - Use SRAM as shared memory

- **Architecture**
    - Write: Poll for an acknowledgment from the remote after each single send operation.

    - Read: Receive packets if they are polled.

- **Limitations**
    - Only support 1 rpmsg endpoint.

    - The mailbox length is limited to 128 bytes.

    - Poll for an acknowledgment after each single send.
    
    - Can only be used for low-frequency data exchange; there is a risk when operating in full-duplex mode

---

- **Linux application**
[rpmsg.c](https://github.com/OpenNuvoton/MA35D1_Linux_Applications/blob/master/examples/rpmsg/rpmsg.c)

    The sample code demonstrates the creation of an endpoint, write function, as well as read after polling function, using Rpmsg-v1 framework.

1. Flow

    - Open the device **rpmsg_ctrl0** 
    - Create a rpmsg endpoint
    - Open the endpoint **rpmsg0**
    - Write a packet to the remote core to start the demo
    - Poll for the acknowledgement after the write operation
    - Poll for the packet reply from the remote core

2. APIs

    Introduction of the APIs that will be used.

*Create endpoint*
```c
    static int rpmsg_create_ept(int rpfd, struct rpmsg_endpoint_info *eptinfo)
```

*Close endpoint*
```c
    clsoe()
```

*Write and poll for ack*
```c
    write()
    poll()
```

*Read if Polled*
```c
    if (poll() > 0)
        read()
```

---

- **RTP application**
[rpmsg_rtp](https://github.com/OpenNuvoton/MA35D1_RTP_BSP/tree/master/SampleCode/OpenAMP/rpmsg_rtp)

1. Flow

    - Create and open the rpmsg endpoint
    - Poll for the packet from the remote core to start the demo
    - Write a packet back to the remote core
    - Poll for the acknowledgement after the write operation

2. APIs

    Introduction of the APIs that will be used.

*Create endpoint*
```c
    int ma35_rpmsg_open(struct rtp_rpmsg *rpmsg, struct rpmsg_endpoint *resmgr_ept, rpmsg_rx_cb_t rxcb, rpmsg_tx_cb_t txcb)
```

*Write & Poll for ack*
```c
    int OPENAMP_send_data(struct rpmsg_endpoint *ept, const void *data, int len)

    int OPENAMP_check_TxAck(struct rpmsg_endpoint *ept)
```

*Read if polled*
```c
    void OPENAMP_check_for_message(struct rpmsg_endpoint *ept)
```

3. User callbacks

*Mailbox Tx callback*
```c
    /**
    * @brief Send data RTP -> A35
    *
    * @param data
    * @param len payload only
    * @return int
    */
    int rpmsg_tx_cb(uint8_t *data, int *len)
```

*Mailbox Rx callback*
```c
    /**
    * @brief Receive data A35 -> RTP
    *
    * @param data
    * @param len payload only
    */
    void rpmsg_rx_cb(uint8_t *data, int len)
```

## Rpmsg-v2

- **Features**
    - Full-duplex

    - SRAM or DRAM can be used as shared memory

    - The mailbox length can be customized from 128B to 16KB

    - Mailbox retransmission mechanism

    - Automatically reconnect if one core disconnects and restarts

    - Expandable command architecture

- **Architecture**

1. Handshake on application layer

    - The ACK packet is appended to the mailbox, allowing full-duplex operation without causing overlap between the send packet and acknowledgment packet.

    - Packet retransmission mechanism is designed to handle situations where the remote ACK packet is not received. This mechanism is also used for detecting disconnections from the remote core.

2. Mailbox format

    Rpmsg-v2 has introduced a mailbox header design. The header consists of a **Subcmd** and **Sequence**, with the format as follows.

    - **SubCMD** field is designed to indicate the type of message contained in the packet. For example, the **START** command is designed for reconnection mechanisms between the dual cores. Users can refer to **START** command to expand and define additional custom commands. 

    - **Sequence** field is designed as a 2nd layer handshake mechanism, serving as the logic for flow control and packet exchange. It helps in ensuring the orderly transmission and reception of packets. For example, packet retransmission mechanisms is based on sequence.

*Tx mailbox*
```t
    [SubCMD](4 bytes) + 
    [TxSEQ of this packet](4 bytes) + [RxSEQ ack to remote](4 bytes) + 
    [reserved](4 bytes) + [payload](up to 16368 bytes)
```

*Rx mailbox*
```t
    [SubCMD](4 bytes) + 
    [RxSEQ sent by remote](4 bytes) + [TxSEQ acked by remote](4 bytes) + 
    [reserved](4 bytes) + [payload](up to 16368 bytes)
```

3. Write

    Check if Tx is not busy before each single send operation.

4. Read if polled

    Perform a periodic read function to check for either send or acknowledgment packet from the remote core.

- **Limitations**

    - Only support 1 rpmsg endpoint

    - The maximum mailbox length cannot be dynamically adjusted and must be predefined in device tree.

---

- **Linux application**
[rpmsg_v2.c](https://github.com/OpenNuvoton/MA35D1_Linux_Applications/blob/master/examples/rpmsg-v2/rpmsg_v2.c)

    The sample code demonstrates two conditions. The first uses a periodic timer to simulate a application where both cores simultaneously send and receive data. The second is in freerun mode, where the two cores exchange data at full speed in a back-and-forth manner. User can switch between periodic mode and freerun mode by:

```c
    #define TXRX_FREE_RUN          0
```

1. Flow

    - Open the device **rpmsg_ctrl0** 
    - Create a rpmsg endpoint
    - Open the endpoint **rpmsg0**
    - Write a **start** command from A35 core to RTP core to start the demo
    - Iterations: Write
    - Iterations: Read if polled

2. APIs

    Introduction of the APIs that will be used.

*Create and open the endpoint*
```c
    int ma35_rpmsg_open(struct ma35_rpmsg *rpmsg, int *fd, struct rpmsg_endpoint_info *info, rpmsg_rx_cb_t rxcb, rpmsg_tx_cb_t txcb)
```

*Close endpoint*
```c
    clsoe()
```

*Write*
```c
    int ma35_rpmsg_send(struct ma35_rpmsg *rpmsg, unsigned char *Tx_Buffer, int *len)
```

*Read if polled*
```c
    int ma35_rpmsg_read(struct ma35_rpmsg *rpmsg, struct pollfd *fds, unsigned char *Rx_Buffer, int *len)
```

*Check Tx finished*
```c
    /**
    * @return 1 : send finished, else : busy
    */
    int rpmsg_tx_acked()
```

*Issue a Tx*
```c
    /**
    * @return 0 : start tx, else : busy
    */
    int rpmsg_tx_trigger()
```

3. User callbacks

*Mailbox Tx callback*
```c
    /**
    * @brief Send data A35 -> RTP
    *
    * @param data
    * @param len payload only
    * @return int
    */
    int rpmsg_tx_cb(unsigned char *data, int *len)
```

*Mailbox Rx callback*
```c
    /**
    * @brief Receive data RTP -> A35
    *
    * @param data
    * @param len payload only
    */
    void rpmsg_rx_cb(unsigned char *data, int len)
```

4. Send packet using a periodic timer

    The periodic mode uses a periodic timer to simulate data transmission, and the frequency of transmission can be modified through:

```c
    #define ACK_TIMER_SEC      0
	#define ACK_TIMER_USEC     100000
```

5. Packet length

    The maximum packet length depends on the definition in DTS. For example, if the reserved memory in DTS is set to 16KB for Tx/Rx shared memory, the packet length is defined as follows:
```c
#define MAILBOX_LEN            0x4000
```

---

- **RTP application**
[rpmsg_rtp](https://github.com/OpenNuvoton/MA35D1_RTP_BSP/tree/master/SampleCode/OpenAMP/rpmsg_rtp)

    User can switch between periodic mode and freerun mode by:
```c
    #define TXRX_FREE_RUN          0
```

1. Flow

    - Create and open the rpmsg endpoint
    - Wait for **START** command sent by the remote A35 core
    - Iterations: Read if polled
    - Iterations: Write

2. APIs

    Introduction of the APIs that will be used.

*Create and open the endpoint*
```c
    int ma35_rpmsg_open(struct rtp_rpmsg *rpmsg, struct rpmsg_endpoint *resmgr_ept, rpmsg_rx_cb_t rxcb, rpmsg_tx_cb_t txcb)
```

*Write*
```c
    int ma35_rpmsg_send(struct rpmsg_endpoint *resmgr_ept, struct rtp_rpmsg *rpmsg, int *len)
```

*Read if polled*
```c
    int ma35_rpmsg_prepare(struct rpmsg_endpoint *resmgr_ept, struct rtp_rpmsg *rpmsg)
```

*Check Tx finished*
```c
    /**
    * @return 1 : send finished, else : busy
    */
    int rpmsg_tx_acked()
```

*Issue a Tx*
```c
    /**
    * @return 0 : start tx, else : busy
    */
    int rpmsg_tx_trigger()
```

3. User callbacks

*Mailbox Tx callback:*
```c
    /**
    * @brief Send data RTP -> A35
    *
    * @param data
    * @param len payload only
    * @return int
    */
    int rpmsg_tx_cb(uint8_t *data, int *len)
```

*Mailbox Rx callback:*
```c
    /**
    * @brief Receive data A35 -> RTP
    *
    * @param data
    * @param len payload only
    */
    void rpmsg_rx_cb(uint8_t *data, int len)
```

4. Send packet using a periodic timer

    The periodic mode uses a periodic timer to simulate data transmission, and the frequency of transmission can be modified through:

```c
    #define ACK_TIMER_HZ        10
```  

---

- **Reference throughput**

    Nuvoton provides reference throughput data for full-duplex mode with mailbox length 16KB, but please note that the throughput may be affected by packet length, user callback functions, and CPU loading.

    Users can test the data throughput in full-duplex or half-duplex operation in this architecture by enabling **TXRX_FREE_RUN**.

```t
    2MB/sec
```

# Summary
This document explains the architecture of the MA35D1-Rpmsg driver. Nuvoton also provides sample codes for inter-processor communication between the A35 core and RTP core, which users can follow for development.

## About Rpmsg-v2

Based on the above description, the advantages of version 2 can be summarized as follows:

- **Full-duplex**

- **Packet retransmission**

- **Communication reconnection**

- **Customized packet size**

- **The handshake and data transmission can be completed simultaneously**

## Start using Rpmsg

Nuvoton provides **remoteproc** to load firmware from the A35 core to the RTP core. Follow the steps below to run rpmsg sample code. Replace **X** with remoteproc instance number, where the default value is 0.

1. Load firmware to RTP core using remoteproc and then execute it.
```cmd
    echo -n <firmware_path> > /sys/module/firmware_class/parameters/path
    echo -n <firmware_rtp.elf> > /sys/class/remoteproc/remoteprocX/firmware
    echo start > /sys/class/remoteproc/remoteprocX/state
```

2. Execute firmware of Linux application on A35 core.
```cmd
    <firmware_a35.bin>
```

3. To stop RTP core, follow the command below.
```cmd
    echo stop > /sys/class/remoteproc/remoteprocX/state
```

## TODOs

- Support for multiple rpmsg endpoints.

- The Linux side application supports pthread, while the RTP side application supports RTOS.


