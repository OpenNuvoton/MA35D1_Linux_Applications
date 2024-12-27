# MA35-AMP
Nuvoton provides inter-processor communication based on the RPMSG framework using OpenAMP.

- **[AMP-M](https://github.com/OpenNuvoton/MA35D1_RTP_BSP/tree/master/SampleCode/OpenAMP/AMP_CoreMRTOS)** : AMP between the core A35 and core CM4 for MA35D1.

- **[AMP-A](https://github.com/OpenNuvoton/MA35D1_NonOS_BSP/tree/master/SampleCode/OpenAMP/AMP_Core1RTOS)** : AMP between the dual-core A35 processors for MA35 family.

# OpenAMP
[OpenAMP](https://github.com/OpenAMP/open-amp?tab=readme-ov-file) is a third-party open-source software that provides inter-core communication interfaces and frameworks such as REMOTEPROC, RPMSG, and VIRTIO. This following document will explain the architecture and usage of MA35-AMP.

## Introduction
MA35-AMP is based on RPMSG, which provides a framework for inter-processor communication and is complemented by IOCTL to make the design more flexible. The architecture uses a single device to manage multiple RPMSG endpoints. 

- **AMP**

    - Device
        ```c
        /dev/rpmsg_ctrl#
        ```

    - Endpoints
        ```c
        /dev/rpmsg#
        ```

- **IOCTL**

    - rpmsg_ctrl
        ```c
        open(), close()
        ```

    - rpmsg endpoints
        ```c
        ioctl(create/destroy), write(), poll(), read()
        ```

# AMP Architecture

The MA35D1 platform features dedicated IPI support between the A35 and CM4 cores via **HWSEM**. However, the dual-core A35 does not have direct hardware support for IPI and instead uses a **timer** interrupt for notifications.

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

    - The user can set the TX and RX shared memory sizes in the Core1/CM4 project, while Core0 Linux reserves the required memory size.

    - Please ensure that the base addresses are the same.

    - Please ensure that the hardware for txipi and rxipi, used to support IPI, are matching.

    - The maximum value for `NO_NAME_SERVICE` should not exceed 32 characters.

- **Limitations**

    - Binding other than one-to-one is not supported.

    <!--- The MA35-AMP driver supports RX queue and TX blocking. However, in cases where the length of the received packet is unknown and the sender's frequency exceeds the receiver's, the RX packet may receive merged packets. The user can design a length field within the packet to avoid this limitation.-->

## Explanation of parameters

The configuration of related parameters is primarily completed on the FreeRTOS side. Linux AMP driver will attempt to parse the remote core and verify its correctness. The following describes all parameters related to **AMP-A**/**AMP-M**, and it is **not** recommended to modify any parameters not mentioned.

- **Core0 device tree:**

    - Reserved memory node for AMP shared memory. The base addess and size of `rpmsg_buf` can be modified according to the application.
        ```dts
        memory-region = <&rpmsg_buf>;
        ```

    - Assign dedicated hardware to support IPI. For example, HWSEM channels 6 and 7 are used as RX/TX IPI in this case.
        ```dts
        rxipi = <&hwsem 6>;
        txipi = <&hwsem 7>;
        ```

- **Core1/CM4 FreeRTOS:**

    - Base address of AMP shared memory.
        ```c
        #define SHARED_RSC_TABLE       ( 0x84000000UL )
        ```

    - Shared memory includes the resource table and the Tx/Rx buffer. `SHARED_BUF_OFFSET` is the memory allocated for the resource table, which is not configurable but must be reserved.
        ```c
        #define SHARED_MEM_PA          ( SHARED_RSC_TABLE + SHARED_BUF_OFFSET )
        ```

    - Size allocated for TX and Rx shared memory.
        ```c
        #define RING_TX_SIZE           ( 0x4000 )
        #define RING_RX_SIZE           ( 0x4000 )
        ```

    - Maximum number of characters used for the name of an endpoint.
        ```c
        #define NO_NAME_SERVICE        ( 32 )
        ```

    - Assign dedicated hardware to support IPI. For example, HWSEM channels 7 and 6 are used as RX/TX IPI in this case.
        ```c
        #define RXIPI_BASE             ( HWSEM0 )
        #define RXIPI_CH_SEL           ( 7 )
        #define TXIPI_BASE             ( HWSEM0 )
        #define TXIPI_CH_SEL           ( 6 )
        ```

    - Number of Rx buffer queues for each Rx endpoint.
        ```c
        #define RXBUF_QUEUE_SIZE       3
        ```

    - Maximum number of Tx or Rx endpoints.
        ```c
        #define VRING_SIZE             8
        ```

## AMP-M driver

[ma35d1_ampm.c](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/drivers/rpmsg/ma35d1_ampm.c)

1. Core0 DTS settings:
[ma35d1.dtsi](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/arch/arm64/boot/dts/nuvoton/ma35d1.dtsi)

Here are the configurations for using AMP-M in Buildroot menuconfig. By default, the image file for the remote core CM4 is `AMP_CoreMRTOS.bin`.
```dts
    Bootloaders  --->
        [*]   Add SCP BL2 image into FIP Image
                Load Image into FIP Image (RTP M4 Image)  --->
        [*]     IPI support
        (0x84000000) Base address of shared memory for AMP
        (0x8800) Size of shared memory for AMP
        (RTP-BSP/AMP_CoreMRTOS.bin) SCP_BL2 binary file names
```

After the build process, the memory region `rpmsg_buf` will be set to the range specified by user in menuconfig.
```dts
    reserved-memory {
        rpmsg_buf: rpmsg_buf@0 {
			reg = <0x0 0x84000000 0x0 0x8800>;
			no-map;
		};
    };
    ampm: ampm {
		compatible = "nuvoton,ma35d1-ampm";
		memory-region = <&rpmsg_buf>;
		rxipi = <&hwsem 6>;
		txipi = <&hwsem 7>;
		status = "okay";
	};
```

2. CM4 settings:
[OpenAMPConfig.h](https://github.com/OpenNuvoton/MA35D1_RTP_BSP/blob/master/SampleCode/OpenAMP/AMP_CoreMRTOS/port/OpenAMPConfig.h)

Please note that the base address `rpmsg_buf` matches `SHARED_RSC_TABLE` and is consistent with the hardware used to support `rxipi` and `txipi`. Since OpenAMP uses resource table as descriptors, the size of the reserved memory must be greater than `RING_TX_SIZE` + `RING_RX_SIZE`. The additional size depends on `VRING_SIZE`, which also represents the maximum number of Tx or Rx endpoints. Generally, reserving **2KB** is sufficient. Driver will also report an warning if the reserved memory is insufficient.
```c
    #define SHARED_RSC_TABLE       ( 0x84000000UL )
    #define RING_TX_SIZE           ( 0x4000 )
    #define RING_RX_SIZE           ( 0x4000 )
    #define NO_NAME_SERVICE        ( 32 ) /* Number of char supported by ns (must be aligned with word) */

    #define RXIPI_BASE             ( HWSEM0 )
    #define RXIPI_IRQ_NUM          (IRQn_Type)HWSEM0_IRQn
    #define RXIPI_CH_SEL           ( 7 )
    #define TXIPI_BASE             ( HWSEM0 )
    #define TXIPI_CH_SEL           ( 6 )
``` 

## AMP-A driver
[ma35_amp.c](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/drivers/rpmsg/ma35_amp.c)

1. Core0 DTS settings:
[ma35xx.dtsi](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/arch/arm64/boot/dts/nuvoton/ma35d1.dtsi)

Here are the configurations for using AMP-A in Buildroot menuconfig. By default, the image file for the Core1 is `AMP_Core1RTOS.bin`.
```dts
    Bootloaders  --->
        [*]   Add SCP BL2 image into FIP Image
                Load Image into FIP Image (A35 Image)  --->
        [*]     IPI support
        (0x84000000) Base address of shared memory for AMP
        (0x8800) Size of shared memory for AMP
        (AMP_Core1RTOS.bin) SCP_BL2 binary file names
        (0x88000000) The execution address of CORE1
        (0x2000000) The execution size of CORE1
```

After the build process, the memory region `rpmsg_buf` will be set to the range specified by user in menuconfig.
```dts
    reserved-memory {
        rpmsg_buf: rpmsg_buf@0 {
            reg = <0x0 0x84000000 0x0 0x8800>
            no-map;
        };
    };
    amp: amp {
        comapaible = "nuvoton,ma35-amp";
        memory-region = <&rpmsg_buf>;
        rxipi = <&timer8>;
        txipi = <&timer9>;
        status = "okay";
    };
```

2. Core1 settings:
[OpenAMPConfig.h](https://github.com/OpenNuvoton/MA35D1_NonOS_BSP/blob/master/SampleCode/OpenAMP/AMP_Core1RTOS/port/OpenAMPConfig.h)

Please note that the base address `rpmsg_buf` matches `SHARED_RSC_TABLE` and is consistent with the hardware used to support `rxipi` and `txipi`. Since OpenAMP uses resource table as descriptors, the size of the reserved memory must be greater than `RING_TX_SIZE` + `RING_RX_SIZE`. The additional size depends on `VRING_SIZE`, which also represents the maximum number of Tx or Rx endpoints. Generally, reserving **2KB** is sufficient. Driver will also report an error if the reserved memory is insufficient.
```c
    #define SHARED_RSC_TABLE       ( 0x84000000UL )
    #define RING_TX_SIZE           ( 0x4000 )
    #define RING_RX_SIZE           ( 0x4000 )
    #define NO_NAME_SERVICE        ( 32 ) /* Number of char supported by ns (must be aligned with word) */

    #define RXIPI_BASE             ( TIMER9 )
    #define RXIPI_IRQ_NUM          (IRQn_ID_t)TMR9_IRQn
    #define TXIPI_BASE             ( TIMER8 )
    #define TXIPI_IRQ_NUM          (IRQn_ID_t)TMR8_IRQn
```

---

# Applications
Nuvoton provides sample code demonstrating inter-processor communication in the MA35-AMP architecture, with Core0 running Linux and either Core1 or CM4 running FreeRTOS, showcasing communication between the dual-core A35 or between the A35 and CM4.

The AMP architecture is based on RPMSG, which uses unified arguments for creating endpoints. The arguments for tx and rx are explained in the comments. Please do **not** modify this structure, even if you change `NO_NAME_SERVICE`, otherwise it will not be accepted by driver.

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

This [sample code](https://github.com/OpenNuvoton/MA35D1_Linux_Applications/blob/master/examples/amp/amp.c) uses pthread to demonstrates 3 tasks and 6 endpoints: The 1st task handles high-frequency short packet data exchange, the 2nd task manages low-frequency long packet data exchange, and the 3rd task demonstrates packet CRC verification.


1. Flow

    - Open the device **rpmsg_ctrl**
    - Create Tx & Rx rpmsg endpoints
    - Open all endpoints
    - Wait until the binding is successful
    - Tx endpoints: write
    - Rx endpoints: poll for event

**Note: The device ID of rpmsg_ctrl may vary depending on the device. Please ensure that #define RPMSG_CTRL_DEV_ID matches the device ID before starting development.**

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
    * @param fd file desc of AMP (rpmsg_ctrl)
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
    * @param fd file desc of AMP (rpmsg_ctrl)
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

## FreeRTOS application 

[AMP_CoreMRTOS](https://github.com/OpenNuvoton/MA35D1_RTP_BSP/tree/master/SampleCode/OpenAMP/AMP_CoreMRTOS), 
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
This document explains the architecture of the MA35-AMP. Nuvoton also provides sample codes for inter-processor communication (IPC) with Core0 running Linux and either Core1 or CM4 running FreeRTOS, which users can follow for development.

## Getting started with AMP-M

**Node: AMP-M is available only for MA35D1 platform with RTP (CM4)**

The CM4 image supports two loading methods: it can be embedded within the kernel image and loaded by Core0 Linux via ARM Trusted Firmware (TFA), or it can be loaded in user space using command through the **remoteproc** driver. Please follow the steps to complete the kernel image.

### Method 1: Loaded by TFA

1. Normal kernel image build using [Buildroot](https://github.com/OpenNuvoton/buildroot_2024).
```cmd
    $ make <ma35d1_defconfig>
    $ make
```

2. Reserved memory for AMP and selected `hwsem` to support IPI. By default, 32KB is reserved; however it will be overriden by step 5. And hwsem 6 and hwsem 7 are used as `rxipi` and `txipi`, respectively.

    [ma35d1.dtsi](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/arch/arm64/boot/dts/nuvoton/ma35d1.dtsi)

```dts
    reserved-memory {
        rpmsg_buf: rpmsg_buf@0 {
			reg = <0x0 0x84000000 0x0 0x8000>;
			no-map;
		};
    };

    ampm: ampm {
		compatible = "nuvoton,ma35d1-ampm";
		memory-region = <&rpmsg_buf>;
		rxipi = <&hwsem 6>;
		txipi = <&hwsem 7>;
		status = "okay";
	};
```

3. Ensure that the configurations in CM4 project match the settings in the DTS.

    [OpenAMPConfig.h](https://github.com/OpenNuvoton/MA35D1_RTP_BSP/blob/master/SampleCode/OpenAMP/AMP_CoreMRTOS/port/OpenAMPConfig.h)

```c
    #define SHARED_RSC_TABLE       ( 0x84000000UL )
    #define RING_TX_SIZE           ( 0x4000 )
    #define RING_RX_SIZE           ( 0x4000 )
    #define SHARED_MEM_SIZE        ( RING_TX_SIZE + RING_RX_SIZE )

    #define RXIPI_BASE             ( HWSEM0 )
    #define RXIPI_IRQ_NUM          (IRQn_Type)HWSEM0_IRQn
    #define RXIPI_CH_SEL           ( 7 )
    #define TXIPI_BASE             ( HWSEM0 )
    #define TXIPI_CH_SEL           ( 6 )
```

4. Prepare for CM4 image and copy binary to `images/RTP-BSP` directory.
```cmd
    $ cp AMP_CoreMRTOS.bin /path/to/buildroot/output/images/RTP-BSP
```

5. In Buildroot's menuconfig, reserve 34 KB (0x8800) for shared memory here. User needs to account for the size of resource table and reserve memory in the DTS that is slightly larger than the `SHARED_MEM_SIZE`. If the reserved shared memory is insufficient, AMP driver will issue a warning message. By default, 2KB is enough. Another example: if user allocates 64 KB for `SHARED_MEM_SIZE`, please reserve 66 KB of shared memory.
```cmd
    $ make menuconfig
    Bootloaders  --->
        [*]   Add SCP BL2 image into FIP Image
                Load Image into FIP Image (RTP M4 Image)  --->
        [*]     IPI support
        (0x84000000) Base address of shared memory for AMP
        (0x8800) Size of shared memory for AMP
        (RTP-BSP/AMP_CoreMRTOS.bin) SCP_BL2 binary file names
```

6. Menuconfig for Kernel
```cmd
    $ make linux-menuconfig
    Device Drivers --->
        Rpmsg drivers --->
        -*- RPMSG device interface
        < > MA35D1 Shared Memory Driver
        <*> MA35D1 AMP-M Driver
        < > MA35 series AMP-A Driver
```

7. Rebuild all
```cmd
    $ make arm-trusted-firmware-dirclean
    $ make arm-trusted-firmware-rebuild uboot-rebuild linux-rebuild; make
```

### Method 2: Loaded by remoteproc

1. Normal kernel image build using Buildroot.
```cmd
    $ make <ma35d1_defconfig>
    $ make
```

2. Reserved memory for AMP and selected `hwsem` to support IPI. By default, 32KB is reserved, please modify it to 34KB (0x8800). And hwsem 6 and hwsem 7 are used as `rxipi` and `txipi`, respectively.

    [ma35d1.dtsi](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/arch/arm64/boot/dts/nuvoton/ma35d1.dtsi)

```dts
    reserved-memory {
        rpmsg_buf: rpmsg_buf@0 {
			reg = <0x0 0x84000000 0x0 0x8800>;
			no-map;
		};
    };

    ampm: ampm {
		compatible = "nuvoton,ma35d1-ampm";
		memory-region = <&rpmsg_buf>;
		rxipi = <&hwsem 6>;
		txipi = <&hwsem 7>;
		status = "okay";
	};
```

3. Menuconfig for Kernel
```cmd
    $ make linux-menuconfig
    Device Drivers --->
        Rpmsg drivers --->
        -*- RPMSG device interface
        < > MA35D1 Shared Memory Driver
        <*> MA35D1 AMP-M Driver
        < > MA35 series AMP-A Driver
```

4. Rebuild all
```cmd
    $ make arm-trusted-firmware-rebuild uboot-rebuild linux-rebuild; make
```

5. Ensure that the configurations in CM4 project match the settings in the DTS.

    [OpenAMPConfig.h](https://github.com/OpenNuvoton/MA35D1_RTP_BSP/blob/master/SampleCode/OpenAMP/AMP_CoreMRTOS/port/OpenAMPConfig.h)

```c
    #define SHARED_RSC_TABLE       ( 0x84000000UL )
    #define RING_TX_SIZE           ( 0x4000 )
    #define RING_RX_SIZE           ( 0x4000 )
    #define SHARED_MEM_SIZE        ( RING_TX_SIZE + RING_RX_SIZE )

    #define RXIPI_BASE             ( HWSEM0 )
    #define RXIPI_IRQ_NUM          (IRQn_Type)HWSEM0_IRQn
    #define RXIPI_CH_SEL           ( 7 )
    #define TXIPI_BASE             ( HWSEM0 )
    #define TXIPI_CH_SEL           ( 6 )
```

6. Prepare the image `AMP_CoreMRTOS.elf` or `AMP_CoreMRTOS.axf` and follow the commands to load it using **remoteproc**. After the CM4 image is loaded, run `amp.bin` to start the demo.
```cmd
    $ echo -n /path/to/AMP_CoreMRTOS > /sys/module/firmware_class/parameters/path
    $ echo -n AMP_CoreMRTOS.elf > /sys/class/remoteproc/remoteproc0/firmware
    $ echo start > /sys/class/remoteproc/remoteproc0/state
    $ /path/to/amp.bin
```

## Getting started with AMP-A

Image of core1, embedded in the kernel image, is loaded by core0 Linux via ARM Trusted Firmware (TFA). Please follow the steps to complete the kernel image.

1. Normal kernel image build using Buildroot.
```cmd
    $ make <ma35xx_defconfig>
    $ make
```

2. Reserved memory for AMP and selected `timer` to support IPI. By default, 32KB is reserved; however it will be overriden by step 5. And timer8 and timer9 are used as `rxipi` and `txipi`, respectively.

    **Note: The hardware that may be used by AMP will be automatically enabled in the subsequent steps.**

    [ma35xx.dtsi](https://github.com/OpenNuvoton/MA35D1_linux-5.10.y/blob/master/arch/arm64/boot/dts/nuvoton/ma35d1.dtsi)

```dts
    reserved-memory {
        rpmsg_buf: rpmsg_buf@0 {
            reg = <0x0 0x84000000 0x0 0x8000>
            no-map;
        };
    };

    amp: amp {
        comapaible = "nuvoton,ma35-amp";
        memory-region = <&rpmsg_buf>;
        rxipi = <&timer8>;
        txipi = <&timer9>;
        status = "okay";
    };
```

3. Ensure that the configurations in Core1 project match the settings in the DTS.

    [OpenAMPConfig.h](https://github.com/OpenNuvoton/MA35D1_NonOS_BSP/blob/master/SampleCode/OpenAMP/AMP_Core1RTOS/port/OpenAMPConfig.h)

```c
    #define SHARED_RSC_TABLE       ( 0x84000000UL )
    #define RING_TX_SIZE           ( 0x4000 )
    #define RING_RX_SIZE           ( 0x4000 )
    #define SHARED_MEM_SIZE        ( RING_TX_SIZE + RING_RX_SIZE )

    #define RXIPI_BASE             ( TIMER9 )
    #define TXIPI_BASE             ( TIMER8 )
```

4. Prepare for Core1 image and copy binary to `images` directory.
```cmd
    $ cp AMP_Core1RTOS.bin /path/to/buildroot/output/images
```

5. In Buildroot's menuconfig, reserve 34 KB (0x8800) for shared memory here. User needs to account for the size of resource table and reserve memory in the DTS that is slightly larger than the `SHARED_MEM_SIZE`. If the reserved shared memory is insufficient, AMP driver will issue a warning message. By default, 2KB is enough. Another example: if user allocates 64 KB for `SHARED_MEM_SIZE`, please reserve 66 KB of shared memory.

    **Note: If you want to change the execution address, remember to also modify the loader in the Core1 project.**

```cmd
    $ make menuconfig
    Bootloaders  --->
        [*]   Add SCP BL2 image into FIP Image
                Load Image into FIP Image (A35 Image)  --->
        [*]     IPI support
        (0x84000000) Base address of shared memory for AMP
        (0x8800) Size of shared memory for AMP
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
        < > MA35D1 Shared Memory Driver
        < > MA35D1 AMP-M Driver
        <*> MA35 series AMP-A Driver
```

7. Rebuild all
```cmd
    $ make arm-trusted-firmware-dirclean
    $ make arm-trusted-firmware-rebuild uboot-rebuild linux-rebuild; make
```

**Note: To use UART16 for debugging, please assign the attribution of UART16 to Core1 in TF-A as follows:**

[ma35d1.dtsi](https://github.com/OpenNuvoton/MA35D1_arm-trusted-firmware-v2.3/blob/master/fdts/ma35d1.dtsi)
```c
    sspcc: sspcc@404F0000 {
        compatible = "nuvoton,ma35d1-sspcc";
            reg = <0x0 0x404F0000 0x0 0x1000>;
            config = <UART0_TZNS>,
                     ...
                     <UART16_TZNS>,
	};
```

## Getting started with AMP-A + AMP-M

The MA35-AMP supports running AMP-A and AMP-M simultaneously, allowing three operating systems to run at the same time. In this case, the Core1 image is loaded via TFA, while the CM4 image is loaded via **remoteproc**. Please follow the steps below to complete the configuration.

1. Normal kernel image build using Buildroot.
```cmd
    $ make <ma35d1_defconfig>
    $ make
```

2. Prepare reserved memory node `rpmsg_buf` for `ampm`, and node `amp_buf` for `amp`.

```dts
    reserved-memory {
        rpmsg_buf: rpmsg_buf@0 {
            reg = <0x0 0x84000000 0x0 0x8800>
            no-map;
        };
        amp_buf: amp_buf@0 {
			reg = <0x0 0x84008800 0x0 0x8800>;
			no-map;
		};
    };

    ampm: ampm {
		compatible = "nuvoton,ma35d1-ampm";
		memory-region = <&rpmsg_buf>;
		rxipi = <&hwsem 6>;
		txipi = <&hwsem 7>;
		status = "okay";
	};

	amp: amp {
		compatible = "nuvoton,ma35-amp";
		memory-region = <&amp_buf>;
		rxipi = <&timer8>;
		txipi = <&timer9>;
		status = "okay";
	};
```

3. Ensure that the configurations in CM4 project match the settings in the DTS. 
```c
    #define SHARED_RSC_TABLE       ( 0x84000000UL )
    #define RING_TX_SIZE           ( 0x4000 )
    #define RING_RX_SIZE           ( 0x4000 )
    #define SHARED_MEM_SIZE        ( RING_TX_SIZE + RING_RX_SIZE )

    #define RXIPI_BASE             ( HWSEM0 )
    #define RXIPI_IRQ_NUM          (IRQn_Type)HWSEM0_IRQn
    #define RXIPI_CH_SEL           ( 7 )
    #define TXIPI_BASE             ( HWSEM0 )
    #define TXIPI_CH_SEL           ( 6 )
```

4. Ensure that the configurations in Core1 project match the settings in the DTS. 

```c
    #define SHARED_RSC_TABLE       ( 0x84008800UL )
    #define RING_TX_SIZE           ( 0x4000 )
    #define RING_RX_SIZE           ( 0x4000 )
    #define SHARED_MEM_SIZE        ( RING_TX_SIZE + RING_RX_SIZE )

    #define RXIPI_BASE             ( TIMER9 )
    #define TXIPI_BASE             ( TIMER8 )
```

4. Prepare for Core1 image and copy binary to `images` directory.
```cmd
    $ cp AMP_Core1RTOS.bin /path/to/buildroot/output/images
```

5. In Buildroot's menuconfig, the based address and size of Core1's shared memory here are set to 0x84008800 and 0x8800, respectively.

```cmd
    $ make menuconfig
    Bootloaders  --->
        [*]   Add SCP BL2 image into FIP Image
                Load Image into FIP Image (A35 Image)  --->
        [*]     IPI support
        (0x84008800) Base address of shared memory for AMP
        (0x8800) Size of shared memory for AMP
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
        < > MA35D1 Shared Memory Driver
        <*> MA35D1 AMP-M Driver
        <*> MA35 series AMP-A Driver
```

7. Rebuild all
```cmd
    $ make arm-trusted-firmware-dirclean
    $ make arm-trusted-firmware-rebuild uboot-rebuild linux-rebuild; make
```

8. Prepare Linux [applications](https://github.com/OpenNuvoton/MA35D1_Linux_Applications/tree/master/examples/amp) for AMP-M and AMP-A. Users can select device ID using `RPMSG_CTRL_DEV_ID`. By default, the device `rpmsg_ctrl0` is assigned to **AMP-M**, and `rpmsg_ctrl1` is assigned to **AMP-A**.

9. Prepare the image `AMP_CoreMRTOS.elf` or `AMP_CoreMRTOS.axf` and follow the commands to load it using **remoteproc**. Then, run the respective `amp.bin` files for AMP-M and AMP-A to start the demo.
```cmd
    $ echo -n /path/to/AMP_CoreMRTOS > /sys/module/firmware_class/parameters/path
    $ echo -n AMP_CoreMRTOS.elf > /sys/class/remoteproc/remoteproc0/firmware
    $ echo start > /sys/class/remoteproc/remoteproc0/state
```

## Version

- AMPM : Version 1.0
- AMPA : Version 1.0
