"""
drivers —— PS 侧外设驱动层

分层约定(改代码前先读这一句):
    cv_regs.py          寄存器地址与位域     —— 单一事实源,不含逻辑
    cv_monitor.py       唯一的 mmio 访问层   —— 不含业务语义
    drivers/*.py        语义层               —— 时序、状态机、保持窗、超时策略
    main.py             装配层               —— 只做"谁先谁后",不含逻辑
    tools/*.py          独立自检脚本         —— 不 import main,可单独上板跑

每个驱动都自带可读的 `status()`,便于 main 与自检脚本统一打印。
"""
