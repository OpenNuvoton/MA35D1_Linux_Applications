# MA35-AMP
Nuvoton provides inter-processor communication between the dual-core A35 processors based on the RPMSG framework using OpenAMP.

# OpenAMP
OpenAMP is a third-party open-source software that provides inter-core communication interfaces and frameworks such as REMOTEPROC, RPMSG, and VIRTIO. This following document will explain the architecture and usage of MA35-AMP.

[OpenAMP](https://github.com/OpenAMP/open-amp?tab=readme-ov-file)

## AMP Architecture
MA35-AMP is based on RPMSG, which provides a framework for inter-processor communication and is complemented by IOCTL to make the design more flexible. The architecture uses a single device to manage multiple RPMSG endpoints. 

- **AMP**

*Device*
```c
    /dev/rpmsg_ctrl0
```
*Endpoints*
```c
    /dev/rpmsg#
```
- **IOCTL**

*rpmsg_ctrl*
```c
    open(), close()
```
*rpmsg endpoints*
```c
    ioctl(create/destroy), write(), poll(), read()
```

# Linux driver
MA35 series does not have a specific hardware support for inter-Processor interrupt (IPI). Instead, it uses a timer interrupt to notify the remote core.

## MA35-AMP driver
[ma35_amp.c](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/drivers/rpmsg/ma35_amp.c)

- **Features**
    - Full-duplex

    - Supports multiple-channel TX and RX endpoints.

    - The size of shared memory and packet length can be customized.

    - Name service feature: In multi-channel/multi-thread applications, the binding between dual-core channels is established through channel names.

    - Dynamically allocate shared memory based on the size requested by the endpoint.

    - When the remote core disconnection is detected, it will automatically attempt to reconnect.

    - Utilizes hardware interrupts to support efficient inter-processor data transfer.

- **Architecture**

1. Endpoints

    - Endpoints are designed as either TX or RX, and users create endpoints based on their purpose.

    - Tx endpoint: Users can register shared memory for use by creating TX endpoints. The registered memory will be released after an endpoint is destroyed.

    - Rx endpoint: Users can create an RX endpoint to receive data from a specific remote endpoint.

2. Name service

    - Each endpoint has its own name. For TX, the name represents the endpoint and indicates that it is waiting for a connection. For RX, the name represents the endpoint it wants to connect to.

    - After an endpoint is created, the driver will attempt to pair it with the corresponding endpoint on the remote core.

3. Reconnection

    - If the remote endpoint is destroyed, the local endpoint will attempt to reconnect until the remote is recreated. Users can also destroy the local endpoint if it is no longer needed.

- **Parameters for shared memory and IPI**

    - The user can set the TX and RX shared memory sizes in the core1 project, while core0 Linux reserves the required memory size.

    - Please ensure that the base addresses are the same.

    - Please ensure that the timers for txipi and rxipi, used to support IPI, are matching.

    - The maximum value for **NO_NAME_SERVICE** should not exceed 32 characters.

1. Core0 DTS settings:
[ma35xx.dtsi](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/arch/arm64/boot/dts/nuvoton/ma35d1.dtsi)
```dts
    reserved-memory {
        amp_buf: amp_buf@0 {
            reg = <0x0 0x84008000 0x0 0x20000> /* 128KB for amp shared memory */
            no-map;
        };
    };
    amp: amp {
        comapaible = "nuvoton,ma35-amp";
        memory-region = <&amp_buf>;
        rxipi = <&timer8>;
        txipi = <&timer9>;
        status = "disabled";
    };
```

2. Core1 settings:
[OpenAMPConfig.h](https://github.com/OpenNuvoton/MA35D1_NonOS_BSP/blob/master/SampleCode/OpenAMP/AMP_Core1RTOS/port/OpenAMPConfig.h)
```c
    #define SHARED_RSC_TABLE       ( 0x84008000UL )
    #define RING_TX_SIZE           ( 0x8000 )
    #define RING_RX_SIZE           ( 0x8000 )
    #define NO_NAME_SERVICE        ( 32 ) /* Number of char supported by ns (must be aligned with word) */

    #define RXIPI_BASE             ( TIMER9 )
    #define RXIPI_IRQ_NUM          (IRQn_ID_t)TMR9_IRQn
    #define TXIPI_BASE             ( TIMER8 )
    #define TXIPI_IRQ_NUM          (IRQn_ID_t)TMR8_IRQn
```

- **Limitations**

    - Binding other than one-to-one is not supported.

    <!--- The MA35-AMP driver supports RX queue and TX blocking. However, in cases where the length of the received packet is unknown and the sender's frequency exceeds the receiver's, the RX packet may receive merged packets. The user can design a length field within the packet to avoid this limitation.-->

---

# Applications
Nuvoton provides sample code for Core0 running Linux and Core1 running FreeRTOS, demonstrating inter-processor communication between the dual-core A35 in the MA35-AMP architecture

The AMP architecture is based on RPMSG, which uses unified arguments for creating endpoints. The arguments for tx and rx are explained in the comments. Please do not modify this structure, even if you change **NO_NAME_SERVICE**, otherwise it will not be accepted by driver.

**Note: To prevent shared memory fragmentation, memory requested by the endpoint is aligned to the appropriate size.**

```c
    struct rpmsg_endpoint_info {
        char name[32]; // Tx: name of local ept; Rx: name of remote ept to bind with
        u32 type;      // EPT_TYPE_TX or EPT_TYPE_RX
        u32 size;      // Tx: request data length in byte; Rx: reserved
    };
```
The sample code provides a command interface to help users quickly understand the MA35-AMP architecture, and it also includes throughput measurement. Please refer to the source code for more details.

## Linux application 

[amp.c](https://github.com/OpenNuvoton/MA35D1_Linux_Applications/blob/master/examples/amp/amp.c)

This sample code uses pthread to demonstrates 3 tasks and 6 endpoints: The 1st task handles high-frequency short packet data exchange, the 2nd task manages low-frequency long packet data exchange, and the 3rd task demonstrates packet CRC verification.


1. Flow

    - Open the device **rpmsg_ctrl0** 
    - Create Tx & Rx rpmsg endpoints
    - Open all endpoints
    - Wait until the binding is successful
    - Tx endpoints: write
    - Rx endpoints: poll for event

2. IOCTL

*create*
```c
    ioctl(int fd, RPMSG_CREATE_EPT_IOCTL, struct rpmsg_endpoint_info eptinfo);
```

*destroy*
```c
    ioctl(int fd, RPMSG_DESTROY_EPT_IOCTL, NULL);
```

*write*
```c
    /**
    * errno
    *   Positive: data length sent
    *   EPERM: remote rx endpoint is closed
    *   EAGAIN: Tx blocking, try again later
    *   EACCES: Remote endpoint is not ready/binded
    */
```

*poll*
```c
    /**
    * events
    *   POLLIN: data received
    *   POLLHUP: remote tx endpoint is closed
    *   EAGAIN: Tx blocking, try again later
    */
```

*read*
```c
    /**
    * return
    *   data length read
    */
```

3. APIs

    Introduction of the APIs that will be used.

*Create endpoint*
```c
    /**
    * @brief Create endpoint
    * 
    * @param fd file desc of AMP (rpmsg_ctrl0)
    * @param amp_ept pointer to an inst of amp_endpoint
    * @return int 
    */
    int amp_create_ept(int *fd, struct amp_endpoint *amp_ept)
```

*Start AMP*
```c
    /**
    * @brief Start AMP
    * 
    * @param fd file desc of AMP (rpmsg_ctrl0)
    * @return int 
    */
    int amp_open(int *fd)
```

*Recycle endpoint*
```c
    /**
    * @brief Recycle endpoint
    * Destroy and close endpoint, call this if you no longer need this endpoint.
    * 
    * @param amp_ept pointer to an inst of amp_endpoint
    * @return int 
    */
    int amp_destroy_ept(struct amp_endpoint *amp_ept)
```

*End AMP*
```c
    /**
    * @brief End AMP
    * 
    * @param fd 
    * @return int 
    */
    int amp_close(int *fd)
```

4. Design your endpoints

    The sample code provides a standard process for creating an endpoint, where all the necessary arguments are listed in this structure. Users can follow this structure to begin the design.

```c
    struct amp_endpoint
    {
        struct rpmsg_endpoint_info eptinfo;
        task_fn_t task_fn; // User designed task
        int fd;
        pthread_t threadHandle;
        void *ept_priv;    // Reserved for passing cookies
    };
```

---

## RTP application 

[AMP_Core1RTOS](https://github.com/OpenNuvoton/MA35D1_NonOS_BSP/tree/master/SampleCode/OpenAMP/AMP_Core1RTOS)

This sample code based on FreeRTOS demonstrates 3 tasks and 6 endpoints: The 1st task handles high-frequency short packet data exchange, the 2nd task manages low-frequency long packet data exchange, and the 3rd task demonstrates packet CRC verification.

1. Flow

    - Create Tx & Rx rpmsg endpoints
    - Open all endpoints
    - Wait until the binding is successful
    - Tx endpoints: write
    - Rx endpoints: poll for event

2. APIs

    Introduction of the APIs that will be used.

*Create a tx endpoint*
```c
    /**
    * @brief Create tx endpoint
    * 
    * @param ept  rpmsg endpoint
    * @param rdev rpmsg device
    * @param name name of endpoint
    * @param size request tx size
    * @return int actual request tx size 
    */
    int ma35_rpmsg_create_txept(struct rpmsg_endpoint *ept, struct rpmsg_device *rdev, const char *name, int size);
```

*Create a rx endpoint*
```c
    /**
    * @brief Create rx endpoint
    * 
    * @param ept  rpmsg endpoint
    * @param rdev rpmsg device
    * @param name name of endpoint
    * @param cb   user rx callback function
    * @return int 0 for success
    */
    int ma35_rpmsg_create_rxept(struct rpmsg_endpoint *ept, struct rpmsg_device *rdev, const char *name, rpmsg_ept_cb cb);
```

*Destroy endpoint*
```c
    /**
    * @brief Destroy rpmsg endpoint
    * 
    * @param ept rpmsg endpoint
    */
    int ma35_rpmsg_destroy_ept(struct rpmsg_endpoint *ept);
```

*Check remote is ready or not*
```c
/**
 * @brief Check if remote is ready
 * 
 * @return int 1 from ready
 */
int ma35_rpmsg_remote_ready(void);
```

*poll*
```c
    /**
    * @brief Receive data by rx endpoint
    *        poll for head or remote close
    *        this function calls user callback if data is ready
    * @param ept 
    * @return "true" for success
    *         "RPMSG_ERR_PERM" if remote endpoint closed
    *         - 1. Do nothing and try reconnecting 2. Destroy and exit
    *         "RPMSG_ERR_NO_BUFF" if rx buffer is full
    *         - Warning: Increase receiver freq. or decrease sender freq.
    */
    int ma35_rpmsg_poll(struct rpmsg_endpoint *ept);
```

*write*
```c
    /**
    * @brief Send data by tx endpoint
    * 
    * @param ept  rpmsg endpoint
    * @param data data to send
    * @param len  data length
    * @return "positive" data length sent
    *         "RPMSG_ERR_PERM" if remote endpoint closed
    *         - 1. Do nothing and try reconnecting 2. Destroy and exit
    *         "RPMSG_ERR_NO_BUFF" if Tx channel is blocking
    *         "RPMSG_ERR_INIT" if remote endpoint is not ready
    */
    int ma35_rpmsg_send(struct rpmsg_endpoint *ept, const void *data, int len);
```

*Check for available and total tx shared memory*
```c
    /**
    * @brief 
    * 
    * @param avail get available buffer size
    * @param total get total buffer size
    * @return int  0 for success
    */
    int ma35_rpmsg_get_buffer_size(int *avail, int *total);
```

*Create endpoint and its task*
```c
    /**
    * @brief Create endpoint and its task
    * 
    * @param rpdev pointer to rpmsg_device
    * @param amp_ept pointer to an inst of amp_endpoint
    * @return int 
    */
    int amp_create_ept(struct rpmsg_device *rpdev, struct amp_endpoint *amp_ept)
```

*Start AMP*
```c
    /**
    * @brief Start AMP
    * 
    * @param rpdev pointer to rpmsg_device
    * @return int 
    */
    int amp_open(struct rpmsg_device *rpdev)
```

*Recycle endpoint and its task*
```c
    /**
    * @brief Recycle endpoint
    * 
    * @param amp_ept pointer to an inst of amp_endpoint
    * @return int 
    */
    int amp_destroy_ept(struct amp_endpoint *amp_ept)
```

*End AMP*
```c
    /**
    * @brief End AMP
    * 
    * @return int 
    */
    int amp_close(void)
```

3. Design your endpoints

    Compared to the structure in Linux, an additional rx callback function is provided. This function will be called when an rx packet is polled, so please do not call this function directly.

```c
    struct amp_endpoint {
        struct rpmsg_endpoint_info eptinfo;
        rpmsg_ept_cb cb;           // Rx endpoint callback
        TaskFunction_t task_fn;    // User designed task
        struct rpmsg_endpoint ept;
        TaskHandle_t taskHandle;
        void *ept_priv;            // Reserved for passing cookies
    };
```

---

# Summary
This document explains the architecture of the MA35-AMP driver. Nuvoton also provides sample codes for inter-processor communication (IPC) between dual-core A35 processors , which users can follow for development.

## Start using MA35-AMP

[Buildroot](https://github.com/OpenNuvoton/buildroot_2024)

Image of core1, embedded in the kernel image, is loaded by core0 Linux via ARM Trusted Firmware (TFA). Please follow the steps to complete the kernel image.

1. Normal kernel image build using Buildroot.
```cmd
    $ make <ma35xx_defconfig>
    $ make
```

2. Reserved memory for AMP and selected timers for IPI support. By default, 128KB is reserved, and timer8 and timer9 are used as rxipi and txipi, respectively.

    **Note: The hardware that may be used by AMP will be automatically enabled in the subsequent steps.**

```dts
    reserved-memory {
        amp_buf: amp_buf@0 {
            reg = <0x0 0x84008000 0x0 0x20000> /* 128KB for amp shared memory */
            no-map;
        };
    };
    amp: amp {
        comapaible = "nuvoton,ma35-amp";
        memory-region = <&amp_buf>;
        rxipi = <&timer8>;
        txipi = <&timer9>;
        status = "disabled";
    };
```

3. Ensure that the configurations in Core1 project match the settings in the DTS. 

    **Note: User needs to account for the size of resource table and reserve memory in the DTS that is slightly larger than the *SHARED_MEM_SIZE*. If the reserved shared memory is insufficient, AMP driver will issue a warning message. By default, 2KB is enough.**

```c
    #define SHARED_RSC_TABLE       ( 0x84008000UL )
    #define RING_TX_SIZE           ( 0x8000 )
    #define RING_RX_SIZE           ( 0x8000 )
    #define SHARED_MEM_SIZE        ( RING_TX_SIZE + RING_RX_SIZE )

    #define RXIPI_BASE             ( TIMER9 )
    #define TXIPI_BASE             ( TIMER8 )
```

4. Prepare for Core1 image and copy binary to **images** directory.

```cmd
    $ cp AMP_Core1RTOS.bin /path/to/buildroot/output/images
```

5. Menuconfig for buildroot.

    **Note: If you want to change the execution address, remember to also modify the loader in the Core1 project.**

```cmd
    $ make menuconfig
    Bootloaders --->
        [*] Add SCP BL2 image into FIP Image
              Load Image into FIP Image (A35 image) --->
        [*]   IPI support
        (AMP_Core1RTOS.bin) SCP_BL2 binary file names
        (0x88000000) The execution address of CORE1
        (0x2000000) The execution size of CORE1
```

6. Menuconfig for Kernel
```cmd
    $ make linux-menuconfig
    Device Drivers --->
        Rpmsg drivers --->
        -*- RPMSG device interface
        <*> MA35 series AMP Driver
```

7. Rebuild all

```cmd
    $ make arm-trusted-firmware-rebuild uboot-rebuild linux-rebuild; make
```

## Version

MA35 series AMP version 1.0
