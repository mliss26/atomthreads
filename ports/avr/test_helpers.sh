test_uart_log=uart.log
test_result_log=atomthreads_test_results.log

test_uart()
{
    uarterm /dev/ttyUSB2 57600 $test_uart_log
}

last_test_line()
{
    tail -1 $test_uart_log
}

log_result()
{
    local testcase=$1
    local result=$2
    echo "$testcase: $result" | tee -a $test_result_log
}

wait_for_go()
{
    local last_line=$(last_test_line)
    while [[ ! "$last_line" =~ Go ]]; do
        last_line=$(last_test_line)
    done
    echo "test running..."
}

wait_for_completion()
{
    local last_line=$(last_test_line)
    while [[ ! "$last_line" =~ Pass|Fail* ]]; do
        last_line=$(last_test_line)
    done
    echo $last_line
}

run_all_tests()
{
    rm -f $test_result_log

    for file in build/*.hex; do
        echo "#"
        echo "# programming $file..."
        echo "#"
        local hex=$(basename $file)
        make program app=${hex/.hex/}
        sleep 1
        make reset && wait_for_go && log_result $hex $(wait_for_completion)
        sleep 1
    done
}
