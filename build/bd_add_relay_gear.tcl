# =============================================================================
#  build/bd_add_relay_gear.tcl
#
#  2026-10-10  用户明确要求 Claude 代写("你直接分配引脚并且接入ps端写完")
#
#  用途:把 relay_ctrl.v 引出的 relay[6:0] / gear[3:0] 接进 BD 顶层,
#        并重新生成 system_wrapper.v。
#
#  🔴 为什么是「删 cell → 同名重建 → 逐条重连」而不是「Refresh Module Reference」
#     —— 这是本工程【第四次】撞上同一个坑,前三次见
#        report/llm-log/LLM-031.md、LLM-044.md 与
#        skill/stale-artifact-silent-reuse/skill.md「形态 3」。
#
#     ① BD 里的 cv_sys_0 是 **module_ref**,它的端口边界【不是】从 cv_sys.v 现读的,
#        而是缓存在:
#            qrs.gen/sources_1/bd/mref/cv_sys/component.xml      (IP-XACT 快照)
#            qrs.runs/synth_1/.gen/sources_1/bd/mref/cv_sys/...  (综合侧的同一份)
#     ② **删掉缓存目录不够** —— BD 文件 system.bd 自己也存了一份引脚列表,
#        重开 BD 只会拿文件里的旧列表 (2026-10-10 实测:清缓存后引脚数仍是 56,
#        relay/gear 仍然查不到 ⇒ 这一步是 Plan A,已证伪,记在这里免得再试)。
#     ③ 官方 `update_module_reference` 在 Vivado 2025.2 批量模式下
#        **存在但返回 rc=1 且错误消息为空**,日志里连一行 ERROR 都没有
#        (2026-10-10 复现:cells/name/绝对路径三种写法全失败,errorInfo 为空)。
#     ⇒ 可靠路径只剩:清缓存 → delete_bd_objs → create_bd_cell 同名重建 → 按 dump 重连。
#        ★ 重建会丢掉 S_AXI 的地址段与 M_AXI 的 DDR 段,**必须显式恢复**,
#          否则 PS 侧 0x43C00000 就没了(症状:Overlay 加载成功但 MMIO 全 DECERR)。
#
#  用法:vivado -mode batch -source build/bd_add_relay_gear.tcl
# =============================================================================

set proj /home/hyp/Desktop/fpga_pro/qrs/qrs.xpr
set rtl  /home/hyp/Desktop/fpga_pro/qrs/qrs.srcs/sources_1/new/relay_ctrl.v
set bdf  /home/hyp/Desktop/fpga_pro/qrs/qrs.srcs/sources_1/bd/system/system.bd

proc log {m} { puts "\[relaybd\] $m" ; flush stdout }
proc die {m} { puts "\[relaybd\] FATAL: $m" ; flush stdout ; exit 1 }

# ---------------------------------------------------------------------------
# 0. 清 mref 缓存(**必须在 open_bd_design 之前**)
#    由 run_bd.sh 在外层做,这里只做断言 —— 缓存还在就直接失败,
#    因为"带着旧缓存重建"正是本工程踩过三次的那个坑。
# ---------------------------------------------------------------------------
foreach c [list \
    /home/hyp/Desktop/fpga_pro/qrs/qrs.gen/sources_1/bd/mref/cv_sys \
    /home/hyp/Desktop/fpga_pro/qrs/qrs.runs/synth_1/.gen/sources_1/bd/mref/cv_sys ] {
    if {[file exists $c]} { die "mref 缓存还在: $c —— 先 rm -rf 再跑本脚本" }
}
log "mref 缓存已清 ✅"

# ---------------------------------------------------------------------------
# 1. 开工程 + 开 BD
# ---------------------------------------------------------------------------
open_project $proj
if {[llength [get_files -quiet $rtl]] == 0} {
    log "add_files relay_ctrl.v"
    add_files -norecurse -fileset sources_1 $rtl
}
open_bd_design $bdf

set old_pins [get_bd_pins -quiet -of_objects [get_bd_cells cv_sys_0]]
log "重建前 cv_sys_0 引脚数 = [llength $old_pins]"

# 记下删之前的 bd_ports(删 cell 不该动它们;下面会逐条断言)
set ports_before [lsort [get_property NAME [get_bd_ports]]]
log "重建前 BD 顶层端口数 = [llength $ports_before]"

# ---------------------------------------------------------------------------
# 2. 删 cell + 3. 同名重建
#    ⚠️ 幂等保护:若 cv_sys_0 已经有 relay 引脚(上一次跑到一半),就跳过重建,
#       否则会把已经连好的线再拆一遍。
# ---------------------------------------------------------------------------
if {[llength [get_bd_pins -quiet cv_sys_0/relay]] > 0} {
    log "cv_sys_0/relay 已存在 —— 跳过删/重建(幂等保护)"
} else {
    log "delete_bd_objs cv_sys_0"
    if {[catch {delete_bd_objs [get_bd_cells cv_sys_0]} err]} { die "删除失败: $err" }
    if {[llength [get_bd_cells -quiet cv_sys_0]] > 0} { die "cv_sys_0 没被删掉" }
    log "  已删除"

    # 断言 bd_ports 还在 —— 若这里挂了,说明得连端口一起重建(本工程历史上没发生,
    # 但暴露出来比后面 get_ports 找不到更好定位)
    set ports_after [lsort [get_property NAME [get_bd_ports]]]
    if {$ports_before ne $ports_after} {
        die "删 cell 之后顶层端口变了。before=[llength $ports_before] after=[llength $ports_after]"
    }
    log "  顶层 [llength $ports_after] 个端口未受影响 ✅"

    log "create_bd_cell -type module -reference cv_sys cv_sys_0"
    if {[catch {create_bd_cell -type module -reference cv_sys cv_sys_0} err]} {
        die "create_bd_cell 失败: $err"
    }
    # 恢复 CONFIG(逐字抄自重建前的 dump)
    set_property -dict [list \
        CONFIG.C_S_AXI_ADDR_WIDTH {8} \
        CONFIG.C_S_AXI_DATA_WIDTH {32} \
        CONFIG.LCD_ID             {0x7016} \
        CONFIG.VERSION_CONST      {0x00010000} \
    ] [get_bd_cells cv_sys_0]
}

set new_pins [get_bd_pins -quiet -of_objects [get_bd_cells cv_sys_0]]
log "cv_sys_0 引脚数 = [llength $new_pins]"

# ★ 这一条是本次改动的**核心判据**:新端口到底进来没有
foreach sig {relay gear} {
    if {[llength [get_bd_pins -quiet cv_sys_0/$sig]] == 0} {
        die "没有 cv_sys_0/$sig —— 缓存没清干净,或 cv_sys.v 没声明该端口"
    }
    log "  ✅ cv_sys_0/$sig 已出现"
}

# ---------------------------------------------------------------------------
# 4. 重连
#
#    🔴 为什么下面【不逐条 die】,而是记下来先跑完再统一验证:
#       2026-10-10 第一次跑时 `connect_bd_net <net> <pin>` 报
#         ERROR: [BD 41-701] connect_bd_net requires at least two pins/ports,
#                              or one pin/port and a net
#       而前一行刚断言过那个 net 存在 —— 也就是说这条命令的入参形态
#       与我预期的不同(Vivado 2025.2)。**猜它的语义不划算**:
#       改成"一律用显式引脚列表(≥2 个端点)"这种无歧义写法,
#       跑完之后【回读网络表】验证,而不是靠命令返回值。
#       ★ 这与 skill/stale-artifact-silent-reuse 的处方一致:
#         判断依据看"回读到的实际结构",不看"命令没说错"。
# ---------------------------------------------------------------------------
set connect_fail {}

# 4a. 时钟 / 复位 —— **只给两个端点:新引脚 + 既有网络里的一个老引脚**
#
#    🔴 2026-10-10 实测结论(probe2,见 report/llm-log/):
#       · 写法① `connect_bd_net <net> <pin>`          → BD 41-701,报错
#       · 写法② 把该网络全部 6 个端点一起喂进去         → 静默失败(被 catch 吞掉),
#             结果 s_axi_aclk 悬空,直到 validate_bd_design 才炸出 BD 41-758
#       · 写法③ **两引脚形式**(下面这个)              → ✅ 成功,
#             回读 net 成员 = FCLK_CLK0 M_AXI_GP0_ACLK aclk slowest_sync_clk
#                            S_AXI_HP0_ACLK s_axi_aclk(6 个,与改造前一致)
#       ⇒ Vivado 自己会把新引脚并进既有的同名网络,**不需要**你把老端点列全。
#         列全反而会因为"这些引脚已经连好了"而失败。
#    ⚠️ 另:`get_property CONNECTED <pin>` 在这里**恒为空** —— 连好了也是空,
#       不能拿它做判据。要看连通性一律用 `get_bd_nets -of_objects <pin>`。
foreach {desc pin netpin} {
    "FCLK_CLK0"          s_axi_aclk    processing_system7_0/FCLK_CLK0
    "peripheral_aresetn" s_axi_aresetn rst_ps7_0_50M/peripheral_aresetn
} {
    if {[catch {connect_bd_net [get_bd_pins cv_sys_0/$pin] [get_bd_pins $netpin]} err]} {
        lappend connect_fail "net $desc: $err"
    } else { log "  net $desc: cv_sys_0/$pin <- $netpin" }
}

# 4b. 两条接口
foreach {desc a b} {
    "S_AXI"  axi_smc/M00_AXI                cv_sys_0/S_AXI
    "M_AXI"  processing_system7_0/S_AXI_HP0 cv_sys_0/M_AXI
} {
    if {[catch {
        connect_bd_intf_net [get_bd_intf_pins $a] [get_bd_intf_pins $b]
    } err]} {
        lappend connect_fail "intf $desc: $err"
    } else { log "  intf $desc 已连($a <-> $b)" }
}

# 4c. 22 根普通外部端口(逐条抄自 2026-10-10 的 BD dump)
set scalar_ports {
    adc_clk adc_oe_n adc_pdn adc_otr adc_d
    dac_cs dac_mosi dac_sclk
    dds_data dds_sclk dds_sync
    lcd_bl lcd_clk lcd_de lcd_hs lcd_rgb lcd_rst lcd_vs
    touch_int touch_rst_n touch_scl touch_sda
}
foreach p $scalar_ports {
    if {[llength [get_bd_ports -quiet $p]] == 0} {
        lappend connect_fail "顶层端口 $p 不存在"
        continue
    }
    if {[catch {
        connect_bd_net [get_bd_ports $p] [get_bd_pins cv_sys_0/$p]
    } err]} {
        lappend connect_fail "port $p: $err"
    }
}
log "  [llength $scalar_ports] 根外部端口已处理"

# ---------------------------------------------------------------------------
#    ⚠️ 注:4d 的全量连通性回读**挪到第 5 步之后**了 —— 见下面「回读校验」一节。
#      (relay/gear 要等 make_bd_pins_external 才有 net,放在这里必然误报)
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# 5. 新端口引出顶层
#    ⚠️ 命令名是 **make_bd_pins_external**,不是 make_external
#       (后者在本版不存在:`invalid command name`,LLM-044 已记)。
# ---------------------------------------------------------------------------
foreach sig {relay gear} {
    if {[llength [get_bd_ports -quiet $sig]] > 0} {
        log "  $sig 已是外部端口,跳过"
        continue
    }
    if {[catch {make_bd_pins_external -name $sig [get_bd_pins cv_sys_0/$sig]} err]} {
        die "make_bd_pins_external $sig 失败: $err"
    }
    log "  make_bd_pins_external $sig"
}

# ---------------------------------------------------------------------------
# 5b. ★ 回读校验:把 cv_sys_0 的**每一个**引脚都查一遍连通性
#
#    为什么不靠上面各步的成功/失败标志:2026-10-10 第一次跑时,时钟连接
#    静默失败(catch 吞掉了),一路跑到 validate_bd_design 才报出
#    「s_axi_aclk 未连到有效时钟源」—— 错误现象离错误动作隔了 5 个步骤,难定位。
#    ⇒ 连完就地全量回读,把"哪根悬空"一次性列出来。
#
#    ⚠️ 两条踩过的判据错误(2026-10-10 本脚本第一版):
#      ① 用 `get_property CONNECTED <pin>` —— **恒为空字符串**,连好了也是空,
#         完全不能用(见 4a 注释)。
#      ② 只查 `get_bd_nets -of_objects <pin>` —— 会把 S_AXI/M_AXI 里的
#         33 根引脚全判成"悬空"。它们走的是 **intf net**,不是标量 net,
#         `get_bd_nets` 根本看不见。⇒ 必须再查 `get_bd_intf_nets`。
# ---------------------------------------------------------------------------
if {[llength $connect_fail] > 0} {
    log "❌ 连接过程报告了 [llength $connect_fail] 个错误:"
    foreach f $connect_fail { log "    · $f" }
}

set dangling {}
foreach pin [get_bd_pins -quiet -of_objects [get_bd_cells cv_sys_0]] {
    # (a) 挂在标量 net 上 → 连通
    if {[llength [get_bd_nets -quiet -of_objects $pin]] > 0} { continue }
    # (b) 属于某个接口,且该接口已连到 intf net → 连通
    set ip [get_bd_intf_pins -quiet -of_objects $pin]
    if {[llength $ip] > 0 && [llength [get_bd_intf_nets -quiet -of_objects $ip]] > 0} { continue }
    lappend dangling [get_property NAME $pin]
}
if {[llength $dangling] > 0} {
    log "❌ cv_sys_0 有 [llength $dangling] 个引脚悬空:"
    foreach d $dangling { log "    · $d" }
    die "悬空引脚未解决,继续下去 validate_bd_design 必然失败"
}
log "cv_sys_0 全部 [llength [get_bd_pins -of_objects [get_bd_cells cv_sys_0]]] 个引脚都已连通 ✅"

# 关键网络的成员回读(与改造前的 dump 逐字比对)
foreach net {processing_system7_0_FCLK_CLK0 rst_ps7_0_50M_peripheral_aresetn} {
    log "  NET $net = [get_property NAME [get_bd_pins -quiet -of_objects [get_bd_nets $net]]]"
}
# 新增两条网络:relay[6:0] / gear[3:0]
# ⚠️ 用「引脚 → 网络」反查,别用 `get_bd_nets relay` —— make_bd_pins_external
#    生成的网络名不保证等于端口名(实测为空 ⇒ get_property 报 Common 17-55 中断脚本)。
foreach sig {relay gear} {
    set n [get_bd_nets -quiet -of_objects [get_bd_pins cv_sys_0/$sig]]
    log "  cv_sys_0/$sig 所在 net = $n (成员: [get_property NAME [get_bd_pins -quiet -of_objects $n]])"
}

# ---------------------------------------------------------------------------
# 6. 恢复地址段
#    ★ 重建 cell 会把这两条一起抹掉,不补就会「Overlay 加载成功但 MMIO 全 DECERR」。
#    ⚠️ 数值不是猜的,是从改动前的 system.bd dump 里读出来的:
#         /processing_system7_0/Data   SEG_cv_sys_0_reg0  offset 0x43C00000  range 4K
#         /cv_sys_0/M_AXI              SEG_..._HP0_DDR_LOWOCM offset 0x00000000 range 1G
# ---------------------------------------------------------------------------
if {[catch {
    assign_bd_address -offset 0x43C00000 -range 4K \
        -target_address_space /processing_system7_0/Data \
        [get_bd_addr_segs cv_sys_0/S_AXI/reg0]
} err]} { die "assign_bd_address (0x43C00000) 失败: $err" }
log "  S_AXI reg0 @ 0x43C00000 ✅"

if {[catch {
    assign_bd_address -offset 0x00000000 -range 1G \
        -target_address_space /cv_sys_0/M_AXI \
        [get_bd_addr_segs processing_system7_0/S_AXI_HP0/HP0_DDR_LOWOCM]
} err]} { die "assign_bd_address (M_AXI DDR) 失败: $err" }
log "  M_AXI -> HP0_DDR_LOWOCM @ 0x00000000 range 1G ✅"

# 断言地址真的落上了 —— 不比"命令没报错"更有说服力的是回读
foreach s [get_bd_addr_segs] {
    log "  SEG $s offset=[get_property OFFSET $s] range=[get_property RANGE $s]"
}

# ---------------------------------------------------------------------------
# 7. 校验 + 保存
# ---------------------------------------------------------------------------
if {[catch {validate_bd_design} err]} { die "validate_bd_design 失败: $err" }
log "validate_bd_design 通过"
save_bd_design

# ---------------------------------------------------------------------------
# 8. 重新生成 wrapper
#    ⚠️ 必须先删旧的:make_wrapper 发现目标已存在时【不会覆盖】——
#       那样 wrapper 里永远没有新端口,而 XDC 会因为 get_ports 找不到 relay[0]
#       报错。症状会指向"约束文件",而不是"wrapper 过期",极易查错方向。
# ---------------------------------------------------------------------------
set old_w [get_files -quiet system_wrapper.v]
if {$old_w ne ""} {
    log "删除旧 wrapper: $old_w"
    catch { remove_files $old_w }
    catch { file delete -force $old_w }
}
generate_target all [get_files $bdf]
if {[catch {make_wrapper -files [get_files $bdf] -top} err]} { die "make_wrapper 失败: $err" }

set new_w [get_files -quiet system_wrapper.v]
if {$new_w eq ""} { die "重新生成后仍找不到 system_wrapper.v" }
log "新 wrapper: $new_w"

# 直接读文件确认端口真的进去了 —— 不靠 make_wrapper 的返回码
set fh [open $new_w r] ; set txt [read $fh] ; close $fh
foreach sig {relay gear} {
    if {[string match "*${sig}*" $txt]} { log "  ✅ wrapper 里有 $sig" } \
    else { die "wrapper 里【没有】$sig —— 端口没引出来" }
}

log "全部完成。"
close_project
exit 0
