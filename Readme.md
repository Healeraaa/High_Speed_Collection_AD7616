# STM32H7相关资料
[【安富莱】STM32H7用户手册](https://www.armbbs.cn/forum.php?mod=viewthread&tid=86980)

### 1.初识STM32H7准备工作，了解Cortex-M7内核以及MDK，IAR，Embedded Studio，STM32CubeIDE和VS比较

本期内容分为三部分：
1、Cortex-M7内核的知识点和相关资料获取。
2、STM32H7的相关知识点和内容获取（参考手册，数据手册，勘误手册和编程手册）。
3、软件生态。
（1）HAL 库软件包介绍。
（2）CMSIS软件包介绍。
（3）STM32CubeMX介绍。
（4）MDK,  IAR, Embedded Stutdio，VS CODE和STM32CubeIDE对比介绍

[1.史上最全面的Cortex-M内核所有资料汇总一览表](https://www.armbbs.cn/forum.php?mod=viewthread&tid=94790)
[2.CMSIS软件包](https://github.com/ARM-software/CMSIS_5)
[3.ST官网](https://www.st.com/content/st_com/en.html)



### 2.STM32H7四通八达的总线矩阵，从系统框架整体把控H7
本期内容分为这么几个内容：
1、第1部分，首先我们了解下整个STM32H7系列生态。
2、第2部分，通过一张图了解H7所有总线和外设的时钟分配。
3、第3部分，系统框图，总线通路（非常重要）。
4、第4部分，AXI总线。
5、第5部分，Flash。
6、第6部分，SRAM。

[1.STM32H7生态](https://www.st.com/en/microcontr )
[2.STM32H7总线矩阵的确是STM32系列里面最复杂的，各个通路，四通八达](https://www.armbbs.cn/forum.php?mod=viewthread&tid=32844)
[3.STM32H7的SRAM和Flash都带ECC校验了](https://www.armbbs.cn/forum.php?mod=viewthread&tid=86777)



### 3.整体捋顺STM32H7的HAL库和LL库的框架，再配合寄存器造轮子找到更适合自己的玩法

本期内容分为几个内容：
1、第1部分，整体介绍HAL库和LL库区别。
2、第2部分，自学HAL库的API怎么个玩法。
3、第3部分，介绍HAL库框架：
（1）介绍HAL库配置文件。
（2）HAL库的时间基准。
（3）HAL启动流程。
（4）外设初始化。
（5）中断方式玩法。
（6）DMA方式玩法。
4、第4部分，寄存器方式造轮子。

[1.实用技能分享，充分利用内联函数，内联汇编，内部函数和嵌入式汇编提升代码执行效率和便捷性](https://www.armbbs.cn/forum.php?mod=viewthread&tid=110134)



### 4.STM32H7从启动到运行过程全解析，电源域，复位，时钟，软硬件启动流程到堆栈，map和htm文件分析

本期内容分为这么几个内容：
1、第1部分，STM32H7整个供电域介绍。
2、第2部分，系统上电硬件启动流程。
3、第3部分，启动文件执行流程。
4、第4部分，整个工程的执行流程。
5、第5部分，堆栈，map和html文件解析。

[1.【烧脑技术贴】无法回避的字节对齐问题，从八个方向深入探讨(变量对齐，栈对齐，DMA对齐，结构体成对齐，Cache, RTOS双堆栈等)](https://www.armbbs.cn/forum.php?mod=viewthread&tid=109400)



### 5.STM32H7的MDK应用专题，系统介绍MDK的调试，AC5，AC6编译器，RTE开发环境和各种配置项作用

本期分为这么几个内容：
1、第1部分，JTAG，SWD,  SWO，ITM,  SWV，RTT， Event Recorder解释。
2、第2部分，MDK简介。
（1）IDE
 (2)  MDK发展
（3）各种MDK版本区别
（4）软件包下载管理
3、第3部分，MDK5最主要的两个变化
（1）AC5和AC6。
（2）RTE开发环境
4、第4部分，MDK入门指南中文版和英文版。
5、第5部分，系统介绍MDK所有配置项。
6、第6部分，MDK基础调试方法。
(1) 全速，单步
(2) 外设寄存器
(3) 全局变量
(4) 局部变量
(5) Flash或者RAM区查看
(6) 断点调试。
(7) 周期性动态更新。



[1.调试相关名字：JTAG，SWD,  SWO，ITM,  SWV，RTT， Event Recorder解释](https://www.armbbs.cn/forum.php?mod=viewthread&tid=110921)
[2.IDE(Integrated Development Environment，集成开发环境)的完成工作流程图](https://www.armbbs.cn/forum.php?mod=viewthread&tid=9605)
[3.MDK Lite， Plus和Pro比较](https://www2.keil.com/mdk5/selector/)
[4.MDK5.29，5.30，5.31，5.32，5.33, 5.34，5.35, 5.36和各种pack软件包镜像下载](https://www.armbbs.cn/forum.php?mod=viewthread&tid=96992)
[5.MDK5中文件感叹号，横杠，灰色，叉号等表示的含义](https://www.armbbs.cn/forum.php?mod=viewthread&tid=76986)
[6.专题：如何做MDK编译器的代码最小优化和性能最佳优化](https://www.armbbs.cn/forum.php?mod=viewthread&tid=1794)
[7.MDK5.33发布，AC6编译DSP库性能重大提升，相比上一版提升6%-50%](https://www.armbbs.cn/forum.php?mod=viewthread&tid=101270)
[8.MDK曾发布的超级给力STM32F1，F407，F429和F7的所有调试方法的设置细节及其注意事项](https://www.armbbs.cn/forum.php?mod=viewthread&tid=14896)
[9.【本坛首发】使用MDK5创建Cortex-M应用指南-中文版](https://www.armbbs.cn/forum.php?mod=viewthread&tid=31288)
[10.使用MDK时包含头文件双引号和尖括号的区别以及选项No Auto Include的作用](https://www.armbbs.cn/forum.php?mod=viewthread&tid=15249)



### 6.MDK专题进阶，Cortex-M内核芯片Hardfault硬件异常调试分析定位
本期内容分为三个部分：
第1部分，全面介绍各种异常的含义。
第2部分，硬件异常代码分析以及部分寄存器硬件自动入栈出栈处理。
第3部分，调试实战分析。

[1、【烧脑技术贴】无法回避的字节对齐问题，从八个方向深入探讨(变量对齐，栈对齐，DMA对齐，结构体成对齐，Cache, RTOS双堆栈等)](https://www.armbbs.cn/forum.php?mod=viewthread&tid=109400)
[2、STM32H7驱动SDRAM两种硬件异常IMPRECISERR和PRECISERR](https://www.armbbs.cn/forum.php?mod=viewthread&tid=95142)



### 7.MDK专题高级进阶，重要的分散加载使用，通过各种实战案例来学习
本期内容分为三个部分：
第1部分，分散加载基础知识。
第2部分，通过各种实战案例学习：
（1）案例1：像使用通用SRAM一样定义使用STM32H7的所有RAM块和外部SDRAM。
（2）案例2：时间关键代码在 ITCM 执行，同时中断向量表也复制了进来
（3）案例3：STM32H7内部Flash和QSPI Flash混合运行程序的方式。
（4）案例4：字库，图库等通过MDK一键下载到外部QSPI/SPI Flash。
第3部分：AC5和AC6的异同。
（1） 官方文档。
（2） 未初始化变量分散加载配置的不同
（3） 分散加载中添加宏定义和#include头文件。
第4部分，通过官方手册，分散加载稍微深入了解.
（1）根域。
（2）.ANY设置不同加载优先级。

// 将数据key_statistics放到RAM_D1段
"__attribute__((section("RAM_D1")))
static uint8_t key_statistics[256];"

相关资料
https://pan.baidu.com/s/1K0eSO25UTzlgqdymQ2rUxw  提取码：oh6l
[1.ARM分散加载文档：](https://developer.arm.com/docume ... using-scatter-files)
[2.AC5和AC6未初始化变量定义方法：](https://developer.arm.com/documentation/ka003046/latest)
[3.STM32H7的MDK汇编启动代码__main，__initial_sp，__Vectors等在C里面的调用方法](https://www.armbbs.cn/forum.php?mod=viewthread&tid=100050)
[4.MDK中根域的含义](https://www.armbbs.cn/forum.php?mod=viewthread&tid=111079)



### 8.MDK专题最后一期，新一代调试技术Event Recorder和RTT，并用STM32CubeMX生成工程模板

本期内容分为四个部分：
第1部分：新一代调试技术的优势
（1）相比于串口打印方式，基于RTT和Event Recorder实现的串口打印，无需占用系统额外的硬件资源，而且API可以在中断和多任务环境中正常调用。
（2）仅需占用SWD调试接口的SWCLK和SWIO即可实现，之前ARM推出的ITM打印方式还需要额外占用一个SWO引脚。
（3）像Event Recorder还额外支持时间测量，功耗测量，CMSIS-RTOS V2封装层及其所有中间件调试信息展示。
第2部分，Event Recoder的用法介绍。
（1）EVR介绍。
（2）EVR事件记录实现。
（3）printf重定向实现。
（4）时间测量实现。
第3部分，RTT的用法介绍。
第4部分，使用STM32CubeMX生成一个最简单的H7裸机模板。
（1）添加EVR和RTT功能。
（2）大家做芯片前期验证时, 仅需一个SWD调试接口就可以做一个工程来测试打印。
[EventRecorder本地参考手册](file:///D:/Keil/Packs/Keil/ARM_Compiler/1.6.3/Doc/EventRecorder/html/er_theory.html#er_req)

[参考文档和相关例子下载](https://www.armbbs.cn/forum.php?mod=viewthread&tid=111133)


### 9.STM32H7的GPIO专题，通过驱动源码，参考手册，数据手册应用笔记系统学习GPIO知识点

本期视频主要分为如下几个部分：
    第1部分：英文版参考手册GPIO章节学习。
        （1）GPIO各种工作模式分析。
        （2）GPIO 的速度等级高的时候， 最好使能 IO 补偿单元。
        （3）施密特触发输入。
        （4）正确理解PA0_C, PA1_C, PC2_C PC3_C。
        （5）H7的上电后GPIO默认已经是模拟模式，而F1，F4是浮空输入。
    第2部分：英文版数据手册GPIO章节学习，了解GPIO的电气特性。
        (1)STM32H7引脚允许的最大拉电流和灌电流
        (2)  STM32H7的弱上拉和下拉电阻大小。
        (3)  STM32H7的GPIO对TTL电平和CMOS电平的兼容问题
    第3部分：GPIO应用笔记手册。
    第4部分：GPIO的HAL库，LL库和寄存器方式玩法。

参考资料：
[1.STM32H7引脚允许的最大拉电流和灌电流](https://www.armbbs.cn/forum.php?mod=viewthread&tid=87665)
[2.H7的上电后GPIO默认已经是模拟模式，而F1，F4是浮空输入](https://www.armbbs.cn/forum.php?mod=viewthread&tid=88677)
[3.STM32H7的SWD调试接口里面的SWDIO-PA13和SWCLK-PA14可以随意使用](https://www.armbbs.cn/forum.php?mod=viewthread&tid=96691)
[4.STM32H7的GPIO对TTL电平和CMOS电平的兼容问题](https://www.armbbs.cn/forum.php?mod=viewthread&tid=87676)
[5.STM32H7开启IO补偿单元和关闭补偿单元，GPIO配置不同速度等级的最高速度](https://www.armbbs.cn/forum.php?mod=viewthread&tid=87677)
[6.施密特触发器也有简单的滤波作用](https://www.armbbs.cn/forum.php?mod=viewthread&tid=111275)
[7.ST曾经发布的一篇GPIO深入介绍文档AN4899，特别是5V耐压的应用介绍值得一看](https://www.armbbs.cn/forum.php?mod=viewthread&tid=91660)

### 10.STM32H7的GPIO专题，非阻塞式驱动编程思想，按键FIFO，蜂鸣器驱动的新式玩法

本期视频教程为大家分享BSP驱动教程第10期，非阻塞式驱动设计思想，我们的按键FIFO，串口FIFO和触摸FIFO都是类似的用法。把这种实现思路做的说明，然后讲解下我们的驱动代码实现。

本期教程主要分为如下几个部分：
    1、硬件设计部分。
        （1）按键硬件设计。
        （2）无源蜂鸣器硬件设计。
    2、阻塞式和非阻塞式编程思想。
        （1）阻塞式设计效果，代码举例测试。
        （2）外部中断式设计效果，代码举例测试。
        （3）非阻塞式设计。
    3、按键FIFO实现。
        （1）FIFO设计思路，看教程文档。
        （2）测试按键效果，感性认识下。
        （3）调试状态FIFO变化，调试看FIFO。
        （4）代码说明。
    4、蜂鸣器驱动新式实现。
参考资料：
[本次视频魔改的例子](https://www.armbbs.cn/forum.php?mod=viewthread&tid=111527)

### 11.STM32H7的GPIO实战，深化非阻塞编程思想，开启Event Recorder狂暴模式，移植驱动到全新器件上

深化非阻塞编程思想，移植驱动到全新器件上，使用杜邦线洞洞板搭建的，不使用我们自己的开发板，来一期GPIO应用实战，并给大家展示下Event Recorder开狂暴模式。
本期视频主要分为如下几个部分：
    第1部分：STM32CubeMX生成工程模板
    第2部分：移植添加BSP相关驱动。
    第3部分：添加Event Recorder的正确姿势，极速printf打印展示。
    第4部分：调试仿真运行。
    第5部分：不同主频的功耗问题。

### 12.STM32H7的NVIC中断管理和优先级动态分配，实战分享减小开关中断对实时性影响，提升系统响应速度
NVIC中断相关的问题，NVIC在项目设计中有着举足轻重的作用，特别是程序较复杂，且中断比较多时。实际应用中推荐：中断越少越好，中断频率越低越好，任务之间耦合度越低越好

本期教程主要分为如下几个部分：
第1部分：图形化实时展示NVIC分组，抢占优先级和子优先级。
    （1）NVIC分组
    （2）抢占优先级和子优先级
    （3）各种优先级配置下，中断响应顺序。
第2部分：中断延迟。
    （1）含义：从中断触发到执行中断服务程序的第一条指令这段时间就是中断延迟时间。
    （2）零中断延迟：并不是说中断延迟时间是0，而是中断触发后，延迟时间接近芯片特性的延迟时间。
第3部分：实战应用
    （1）实战应用场景一：尽量不要使用全局开关中断，使用局部中断
    （2）实战应用场景二：使用__set_PRIMASK（操作PRIMASK寄存器）做全局开关中断的地方，改用__set_BASEPRI（操作BASEPRI寄存器）
    （3）实战应用场景三：降低中断服务程序执行时间。
第4部分：几十个中断时，中断动态优先级分配问题。
    在不同优先级分配下，总有一个功能无法正常运行，而修改后，这个问题修复了，但又出现新的问题。所以就想搞搞动态分配，发现不实用，问题更多。

参数资料：
[实战技能分享，减小开关中断对系统实时性的影响，提升系统响应速度](https://www.armbbs.cn/forum.php?mod=viewthread&tid=109927)

### 13.系统讲解Cortex-M7内核Cache和MPU
初学时容易无法形成系统的认识，说到某一个知识点也明白，但是具体到Cache读写操作的时候是怎么个流程，就懵了，本期视频教程就帮大家捋顺这个问题。MPU和Cache也是M7内核芯片学习的核心内容。

本期教程主要分为如下几个部分：
第1部分：MPU基础
    （1）MPU和Cache作用介绍。
    （2）深入介绍MPU。
第2部分：Cache各种配置工作流程理解。
    （1）深入介绍各个Cache配置策略执行流程。

参数资料：
[1.言简意赅的介绍M7内核的Cache工作流程，摸爬滚打半年的经验总结  ](https://www.armbbs.cn/forum.php?mod=viewthread&tid=90066)
2、https://www.sciencedirect.com/topics/computer-science/set-associative-cache
3、文档链接：https://pan.baidu.com/s/1z6A9fWDCp2_BKlEIQLNoRg 提取码：7x4q


### 14.超干货，MPU和Cache实战，一张图了解所有经典配置案例，争取人人都可以玩溜
[一张图](https://www.armbbs.cn/forum.php?mod=viewthread&tid=112268)


### 15.几张手册框图讲明白DMAMUX和DMA各种玩法，FIFO，突发传输，不同位宽传输方式等
本期教程主要分为如下几个部分：
第1部分：整体把控
    DMAMUX1，DMAMUX2，DMA1，DMA2在总线上的位置以及相关DMA支持访问的相关外设。
第2部分：DMAMUX介绍
    （1）为什么使用DMAMUX，相比之前方案的优势。
    （2）DMAMUX的外设请求，同步输入和触发输入。
第3部分：DMA和BDMA介绍
    （1）先简单看框图
    （2）通过程序来说明每个配置项。
第4部分：
    DMA到底有没有加速作用。

参考资料：
[DMA到底能不能起到加速程序执行的作用，DMA死等操作是否合理，多个DMA数据流同时刷是否处理过来](https://www.armbbs.cn/forum.php?mod=viewthread&tid=109765)


### 16.DMA双缓冲实现32路脉冲并行同步控制
本期教程为大家分享DMA双缓冲实现32路脉冲并行同步控制，同时这种玩法也支持任意IO做脉冲控制。

参考资料：
1、例子下载：
链接：https://pan.baidu.com/s/1fxdlinf8Jerw885zjAdKEA 提取码：wsdk

2、手册：
在之前文档版教程第40到41章的基础上做了扩展：
https://www.armbbs.cn/forum.php?mod=viewthread&tid=86980

### 17.单片机bootloader专题，启动，跳转配置和调试下载的各种用法
本期教程开始给大家分享各种BootLoader专题视频教程，争取把市面上各种BootLoader玩法都做个实战讲解。像系统BootLoader，网络boot，USB boot，串口boot，SD卡boot等常用玩法都整理下，还有裸机版和RTOS版本的app+boot调试下载问题都做下。

[1、实战技能分享，一劳永逸的解决BOOT跳转APP失败问题，含MDK AC5，AC6和IAR，同时制作了一个视频操作说明](https://www.armbbs.cn/forum.php?mod=viewthread&tid=109595)

[2、成功实现MDK自动生成hex文件的crc值并附加到hex文件末尾（bin也支持），然后跟STM32的硬件CRC计算值做比较](https://www.armbbs.cn/forum.php?mod=viewthread&tid=97379)

[3、【不是问题的问题】为什么STM32的Flash地址要设置到0x08000000](https://www.armbbs.cn/forum.php?mod=viewthread&tid=109321)

[5、OpenBLT](https://www.armbbs.cn/forum.php?mod=viewthread&tid=99063)

[6、MCUBoot](https://www.armbbs.cn/forum.php?mod=viewthread&tid=29999)

[7、wolfboot](https://www.armbbs.cn/forum.php?mod=viewthread&tid=99992)

### 18.基于NAND，eMMC，SD卡和U盘的BootLoader实战，带CRC完整性校验
本期教程分享的工程是可以用于项目实战的，大家可以根据自己的需要做魔改，不限制单片机平台。

参考资料：
1、例子下载：
链接：https://pan.baidu.com/s/1zIA4bv89NicOiOfYZsFmPg 提取码：vakh

[2、系统的谈下STM32H7的SD卡驱动兼容性问题，现在兼容性很强，主流厂家16GB以上容量全部通过](https://www.armbbs.cn/forum.php?mod=viewthread&tid=107689)

[3、uCOS全家桶带的CRC软件包，含汇编加速方式，支持查表和计算两种方式](https://www.armbbs.cn/forum.php?mod=viewthread&tid=97690)

[4、实战技能分享，一劳永逸的解决BOOT跳转APP失败问题，含MDK AC5，AC6和IAR，同时制作了一个视频操作说明](https://www.armbbs.cn/forum.php?mod=viewthread&tid=109595)

[5、成功实现MDK自动生成hex文件的crc值并附加到hex文件末尾（bin也支持），然后跟STM32的硬件CRC计算值做比较](https://www.armbbs.cn/forum.php?mod=viewthread&tid=97379)

### 19.单片机BootLoader的AES加密实战，含上位机和下位机代码全开源
本期教程，继续为大家带来BootLoader专题进阶，APP固件的AES加密实现，提升产品安全性，一定程度上防止被轻易破解。

参考资料：
1、例子下载：https://pan.baidu.com/s/1olJIWk3orltxxJ0Kpxa6iw  提取码：j2fi 

[2、bin编辑免费小软件：](https://www.hhdsoftware.com/free-hex-editor)

[3、最强bin固件逆向，可方便查看每个外设的配置实现，代码安全任重道远](https://www.armbbs.cn/forum.php?mod=viewthread&tid=111237)