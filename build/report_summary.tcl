# ============================================================================
#  build/report_summary.tcl —— 把 M3「综合与实现报告」要回填的那几个数取出来
# ============================================================================
#
#  单独跑(工程已实现过,只想重取一遍汇总;**不动综合结果**):
#
#      vivado -mode batch -notrace \
#          -source build/report_summary.tcl \
#          -tclargs $HOME/Desktop/fpga_pro/qrs        # 工程目录,可省
#
#  也由 `build/build_bitstream.tcl` 在实现完成之后 `source` 进来复用。
#  输出:`$QRS_OUT_DIR/summary.txt`(不给就落在当前目录)。
#
#  ⛔ **本文件必须能在"已经 open_run 过"和"没 open_run"两种情况下都跑对** ——
#     单独跑时要自己 open_run impl_1;被 source 时外面已经开好了。
# ============================================================================

# ---- 0. 工程 / 输出位置 -----------------------------------------------------
# 单独跑时:用 -tclargs 给的工程目录打开;没给就退回环境变量 / 默认路径。
if {[info exists ::argv] && ([llength $::argv] > 0)} {
    set proj_dir [lindex $::argv 0]
} elseif {[info exists ::env(QRS_PROJ_DIR)]} {
    set proj_dir $::env(QRS_PROJ_DIR)
} else {
    set proj_dir "$::env(HOME)/Desktop/fpga_pro/qrs"
}

# 判据:外面已经 open 过实现结果的话就复用,不重复开。
# ⛔ **`current_design` 在没有工程时会直接抛异常**(`[Common 17-53] No open project`),
#    不是返回空串 —— 2026-10-14 踩的。所以必须用 `catch` 包住,不能只比空串。
set _have_design 0
if {![catch {set _d [current_design]} _err]} {
    if {$_d ne ""} { set _have_design 1 }
}

if {$_have_design} {
    puts "== report_summary:复用外面已打开的实现结果([current_design])"
} else {
    puts "== report_summary:自己打开工程 $proj_dir/qrs.xpr"
    open_project "$proj_dir/qrs.xpr"
    open_run impl_1
}

if {[info exists ::env(QRS_OUT_DIR)]} {
    set out_dir $::env(QRS_OUT_DIR)
} else {
    set out_dir [pwd]
}
set fh [open [file join $out_dir summary.txt] w]

puts $fh "== QRS build summary(doc: build/build_bitstream.tcl / report_summary.tcl)=="

# ---- 1. 时序 ----------------------------------------------------------------
# WNS 为负 = 时序不收敛,位流不可信。
# ⛔ `-delay_type` 与 `-setup` / `-hold` **互斥** —— 2026-10-14 踩的:
#    `ERROR: [Vivado 12-608] -delay_type cannot be used with -setup/-hold`。
#    两者只能给一个;这里统一走 `-delay_type max|min`。
set wns [get_property SLACK [get_timing_paths -delay_type max]]
set whs [get_property SLACK [get_timing_paths -delay_type min]]
puts $fh [format "WNS = %+.3f ns" $wns]
puts $fh [format "WHS = %+.3f ns" $whs]

# TNS / THS / 失败端点 / 每时钟组 —— 直接**筛 `report_timing_summary` 的正文行**,
# 不自己用 `get_timing_paths` 聚合口径(聚合容易和报告对不上,报告才是要交的那份)。
#
# ⛔ 过滤规则要**宽容**,不要按固定列宽写正则 —— 2026-10-14 第一版按
#    `^\s{2}(clk_\S+)\s+...` 写,结果只捞到一行(表里各行的前导空格数并不一致),
#    而且 `Total Negative Slack (TNS)` 这种措辞根本不在这个报告的正文里
#    (它在 `report_timing`,不在 `report_timing_summary`)。
#    ⇒ 改成三条宽松判据:① 以 `clk_` 开头的行 ② 一行里全是数字的汇总行
#       ③ 那句"约束全部满足"的结论。
set tsum [report_timing_summary -return_string]
puts $fh "-- 时序(report_timing_summary,筛过的正文行)--"
foreach line [split $tsum "\n"] {
    set t [string trim $line]
    if {$t eq ""} { continue }

    # ① 每时钟组的 WNS/WHS/失败端点行
    # ② `Design Timing Summary` 那条汇总行 —— **必须是它**:
    #    「失败端点 **0 / 5469**」里的 5469 只在这里,不在下面的每时钟组表里
    #    (每时钟组各算各的端点,加起来 4672 ≠ 5469 —— 组与组**不覆盖全设计**)。
    #    这条行**同时含小数和整数**(`0.354 0.000 0 5469 …`),
    #    第一版按"12 个浮点数"写正则 ⇒ 逮不着。改成**数数字令牌个数**。
    # ⚠️ `split $t " "` 在**连续空格**处会吐出空令牌 ⇒ 不能用「令牌数 == 数字数」判。
    #    先滤掉空令牌,再看是不是"整行都是数字"。
    set toks {}
    foreach tok [split $t " "] { if {$tok ne ""} { lappend toks $tok } }
    set ntok [llength $toks]
    set all_num 1
    foreach tok $toks { if {![regexp {^-?[0-9.]+$} $tok]} { set all_num 0; break } }
    if {[regexp {^clk_\S+} $t] ||
        ($ntok >= 8 && $all_num) ||
        [string match "*timing constraints are met*" $t]} {
        puts $fh "  [regsub -all {\s+} $t " "]"
    }
}

# 时钟摘要 —— 报「设计值 vs 综合出来的值」时要有出处
puts $fh "-- 时钟(report_clocks)--"
foreach line [split [report_clocks -return_string] "\n"] {
    set t [string trim $line]
    if {[regexp {^clk_\S+} $t]} {
        puts $fh "  [regsub -all {\s+} $t " "]"
    }
}

# ---- 2. 资源 ----------------------------------------------------------------
# 直接从 `report_utilization` 的文本里取那几行,不自己数 primitive ——
# 自己数容易和报告口径不一致(报告里的 "CLB LUTs" = LUT as Logic + as Memory)。
# ⛔ 标签名按 **Vivado 2025.2 的实际用词**写,不要照抄旧版手册 ——
#    这个版本的报告行是 `Slice LUTs` / `Slice Registers`(不是 `CLB LUTs` / `CLB Registers`),
#    记忆块是 `RAMB36/FIFO` / `RAMB18`。把两套名字都放进候选集,换版本也不至于静默漏行。
# ⚠️ 同一个标签会在报告里出现两次(`Slice Logic` 摘要表 + `CLB Logic` 明细表)
#    ⇒ 同一行内容**去重**,否则 summary.txt 里 `Slice Registers` 连着印两遍,
#      下一个人会以为是两条不同的数。
puts $fh "-- 资源(report_utilization)--"
# `RAMB36` 后面可能跟 `/FIFO` ⇒ 尾巴要用 `[^|]*` 吃掉,不能只写 `\s*\|`。
set want {CLB LUTs|Slice LUTs|CLB Registers|Slice Registers|Block RAM Tile|RAMB36[^|]*|RAMB18[^|]*|DSPs|Bonded IOB|BUFGCTRL}
set seen {}
foreach line [split [report_utilization -return_string] "\n"] {
    if {[regexp "\\|\\s*($want)\\s*\\|" $line]} {
        set row [regsub -all {\s+} [string trim $line] " "]
        if {[lsearch -exact $seen $row] >= 0} { continue }
        lappend seen $row
        puts $fh "  $row"
    }
}

# ---- 3. DRC -----------------------------------------------------------------
# 🔴 **Error 必须为 0** —— 非 0 就不能拿这个位流上板。
# Warning 要**逐条记账**(哪条规则、几条、落在哪个模块),既往有:
#   CHECK-3  = "报告条数达上限",是元信息不是设计缺陷
#   REQP-1839/1840 = RAMB 地址脚由带异步复位的寄存器驱动(见 docs/project_state.md)
puts $fh "-- DRC(report_drc)--"
set drc [report_drc -return_string]
foreach line [split $drc "\n"] {
    if {[regexp {^\|\s*(CHECK|REQP|RTSTAT|NSTD|LUTAR|TIMING)-[0-9]+\s*\|\s*(Error|Critical Warning|Warning)} $line]} {
        puts $fh "  [regsub -all {\s+} [string trim $line] " "]"
    }
}
foreach line [split $drc "\n"] {
    if {[regexp {^[0-9]+ (Error|Critical Warning)} $line]} {
        puts $fh "  ⛔ [string trim $line]"
    }
}

puts $fh "== end =="
close $fh

puts "== 汇总写到 [file join $out_dir summary.txt]"
