# ============================================================================
#  2026-10-05  DAC1220(偏压) + AD9833(激励) 接线 —— Block Design 侧改动
# ----------------------------------------------------------------------------
#  为什么需要这个脚本
#  ------------------
#  RTL 侧 cv_sys.v 新增了 6 个输出端口,并把 AXI4-Lite 地址宽度从 7 位扩到 8 位
#  (cv_axi_lite_slave.v,为了给 DDS_FREQ 腾出 0x80)。
#  但 BD 里的 cv_sys_0 是 **module_ref(HDL 模块引用)**,不是打包 IP ——
#  BD 把旧的端口清单和 ADDR_WIDTH=7 缓存住了。不刷新就看不到新端口,
#  make_bd_pins_external 也无从下手。
#
#  实测证据(2026-10-05):
#    system.bd 里 cv_sys_0/S_AXI 的 ADDR_WIDTH = "7"(value_src = constant);
#    get_bd_pins -of_objects [get_bd_cells cv_sys_0] 里【没有】dac_*/dds_*。
#
#  脚本做 5 件事
#  -------------
#    1. 把 io.xdc 从工程移除(它的 IS_ENABLED=0,内容已并入 led.xdc)
#    2. 刷新 cv_sys_0 的 module reference(拾回 ADDR_WIDTH=8 与 6 个新端口)
#    3. 把 6 个新端口引出成顶层端口(名字必须与 led.xdc 里的一致)
#    4. validate + 保存 BD
#    5. 重新生成 BD 产物(否则综合仍用旧网表)
#
#  用法(二选一)
#  ------------
#    · GUI:Tcl Console 里
#          source {/home/hyp/Desktop/qrs_2026_fpga/build/bd_add_dac_dds.tcl}
#    · 批处理:
#          vivado -mode batch -source .../bd_add_dac_dds.tcl
#
#  ⚠️ 工程目录不是 git 仓库 —— 没有回滚。执行前建议整目录备份一份。
#  ⚠️ 脚本可重复执行(幂等):端口已引出、io.xdc 已移除都会跳过。
# ============================================================================

set PROJ_PATH   /home/hyp/Desktop/fpga_pro/qrs/qrs.xpr
set BD_FILE_NAME system.bd
set CVSYS_CELL  cv_sys_0
set NEW_PINS    {dac_sclk dac_cs dac_mosi dds_sclk dds_sync dds_data}

proc sec  {t} { puts "\n======== $t ========" }
proc ok   {t} { puts "  \[OK\]   $t" }
proc skip {t} { puts "  \[SKIP\] $t" }
proc bad  {t} { puts "  \[FAIL\] $t" }

# ---------------------------------------------------------------------------
#  0) 打开工程 / BD(已打开则复用)
# ---------------------------------------------------------------------------
sec "0) 环境"

if {[catch {set p [current_project -quiet]}] || ![llength $p]} {
    open_project $PROJ_PATH
    ok "已打开工程:$PROJ_PATH"
} else {
    ok "复用已打开工程:[get_property NAME $p]"
}

if {[catch {set b [current_bd_design -quiet]}] || ![llength $b]} {
    open_bd_design [get_files $BD_FILE_NAME]
    ok "已打开 BD:$BD_FILE_NAME"
} else {
    ok "复用已打开 BD:[get_property NAME $b]"
}

# ---------------------------------------------------------------------------
#  1) 移除 io.xdc
# ---------------------------------------------------------------------------
sec "1) 移除 io.xdc(IS_ENABLED=0 的僵尸约束)"

# ⚠️ get_property NAME 对 file 对象返回的是【全路径】,不是文件名 ——
#    必须 file tail,否则永远比不中(2026-10-05 实测踩过)。
set iox {}
foreach f [get_files -quiet "*.xdc"] {
    set fn [file tail [get_property NAME $f]]
    if {[string match "*led.xdc" $fn] || [string match "*io.xdc" $fn]} {
        puts "    工程内约束文件:$fn"
    }
    if {$fn eq "io.xdc"} { lappend iox $f }
}

if {![llength $iox]} {
    skip "io.xdc 不在工程里(可能已执行过)"
} else {
    remove_files $iox
    ok "已从工程移除(磁盘文件保留;备份在 qrs_2026_fpga/docs/_retired/)"
}

# ---------------------------------------------------------------------------
#  2) 刷新 module reference
# ---------------------------------------------------------------------------
sec "2) 刷新 $CVSYS_CELL 的 module reference"

set cell [get_bd_cells -quiet $CVSYS_CELL]
if {![llength $cell]} { error "BD 里找不到 $CVSYS_CELL —— 确认 BD 是否正确" }

# ⚠️ 这个命令收的是【IP 实例】(get_ips),不是 bd_cell。
#    传 get_bd_cells 的结果会返回 rc=1 且错误消息为空 —— 极难看懂。
#    2026-10-05 实测:传 get_ips 对象才 rc=0 且刷新生效。
set ip [get_ips -quiet *$CVSYS_CELL*]
if {![llength $ip]} { error "找不到 $CVSYS_CELL 对应的 IP 实例" }
puts "    目标 IP 实例:[get_property NAME $ip]"

set aw_before [get_property -quiet CONFIG.ADDR_WIDTH [get_bd_intf_pins $CVSYS_CELL/S_AXI]]
puts "    刷新前:S_AXI ADDR_WIDTH = $aw_before"

update_module_reference $ip

set aw_after [get_property -quiet CONFIG.ADDR_WIDTH [get_bd_intf_pins $CVSYS_CELL/S_AXI]]
puts "    刷新后:S_AXI ADDR_WIDTH = $aw_after"

if {$aw_after eq "8"} {
    ok "地址宽度已变成 8"
} else {
    bad "地址宽度仍是 $aw_after —— 没有刷新成功"
}

# ---------------------------------------------------------------------------
#  3) 引出 6 个新端口
# ---------------------------------------------------------------------------
sec "3) 引出 6 个新端口"

foreach n $NEW_PINS {
    if {![llength [get_bd_pins -quiet $CVSYS_CELL/$n]]} {
        bad "$n 在 $CVSYS_CELL 上不存在 —— module reference 没刷新出来"
        continue
    }
    if {[llength [get_bd_ports -quiet $n]]} {
        skip "$n 已是顶层端口"
        continue
    }

    set before {}
    foreach p [get_bd_ports -quiet] { lappend before [get_property NAME $p] }

    make_bd_pins_external [get_bd_pins -quiet $CVSYS_CELL/$n]

    # ⚠️ 自动命名会撞上 BD 内部【已有的同名 net】,给端口名加一个 _0 后缀
    #    (实测:dac_sclk 引出后叫 dac_sclk_0)。这样 led.xdc 里的
    #    [get_ports dac_sclk] 会【全部落空】—— 综合报 NSTD-1/UCIO-1。
    #    所以这里强制改回 XDC 用的名字。
    set created ""
    foreach p [get_bd_ports -quiet] {
        if {[lsearch -exact $before [get_property NAME $p]] < 0} { set created $p; break }
    }
    if {$created eq ""} { bad "$n 引出后找不到新建端口"; continue }

    set cn [get_property NAME $created]
    if {$cn ne $n} {
        set_property name $n $created
        ok "$n 已引出(自动名 $cn → 改名 $n)"
    } else {
        ok "$n 已引出"
    }
}

# ---------------------------------------------------------------------------
#  4) 校验顶层端口名 + 地址段
# ---------------------------------------------------------------------------
sec "4) 校验"

set missing 0
foreach n $NEW_PINS {
    if {![llength [get_bd_ports -quiet $n]]} { bad "顶层端口 $n 缺失"; incr missing }
}
if {!$missing} { ok "6 个顶层端口名与 led.xdc 完全对齐" }

puts "    ---- 当前 BD 顶层端口 ----"
foreach p [get_bd_ports -quiet] { puts "      [get_property NAME $p]" }

puts "    ---- $CVSYS_CELL 地址段 ----"
foreach seg [get_bd_addr_segs -quiet -of_objects [get_bd_addr_spaces -quiet]] {
    if {[string match "*$CVSYS_CELL*" $seg]} {
        puts "      $seg  offset=[get_property -quiet OFFSET $seg]  range=[get_property -quiet RANGE $seg]"
    }
}

# 地址段范围由地址编辑器算出。ADDR_WIDTH 7→8 后,**若段范围仍是 128 字节(0x80)**,
# 就装不下 DDS_FREQ 所在的 0x80 —— 必须重算。
set small 0
foreach seg [get_bd_addr_segs -quiet *$CVSYS_CELL*] {
    set off [get_property -quiet OFFSET $seg]
    set rng [get_property -quiet RANGE $seg]
    puts "      $seg  offset=$off  range=$rng"
    if {$rng ne "" && $rng < 256} { set small 1 }
}
if {$small} {
    bad "地址段范围 <256 字节,装不下 0x80 —— 需要重新分配地址"
} else {
    ok "地址段范围足够(>=256 字节)"
}

# ---------------------------------------------------------------------------
#  5) validate + 保存 + 重新生成
# ---------------------------------------------------------------------------
sec "5) validate + 保存 + 重新生成"

validate_bd_design
save_bd_design
ok "BD 已保存"

generate_target all [get_files $BD_FILE_NAME]
ok "BD 产物已重新生成"

puts "\n======== 完成 ========"
puts "  下一步(Vivado GUI 里手动做):"
puts "    1. Generate Output Products(若上面 generate_target 未覆盖)"
puts "    2. Create HDL Wrapper(如提示 out-of-date)"
puts "    3. Run Synthesis -> Implementation -> Generate Bitstream"
puts "    4. 导出新的 .bit / .hwh 到 qrs_2026_fpga/board/"
