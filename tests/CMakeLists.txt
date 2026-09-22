function(yigo_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE yigo_core Qt5::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

yigo_add_test(tst_board)
yigo_add_test(tst_rules)
yigo_add_test(tst_gametree)
