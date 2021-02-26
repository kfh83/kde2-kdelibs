#=============================================================================
# Copyright 2017       Helio Chissini de Castro <helio@kde.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

if(BUILD_TESTS)
    set(kio_tests
        speed
        kdefaultprogresstest
        kclipboardtest

        # manual tests/needs test data
        #kioslavetest
        #kshredtest
        #kionetrctest
        )

    set(ksycoca_tests
        kurifiltertest
        ksycocatest
        getalltest
        kfiltertest
        kscantest

        # manual tests/needs test data
        #kruntest
        #kiopassdlgtest
        #kdirlistertest
        #previewtest
        #kmimemagictest
        #ktradertest
        )

    # Requires test files
    add_executable(ktartest ktartest.cpp)
    target_link_libraries(ktartest kde2::ksycoca)
    add_test(NAME ktartest COMMAND ktartest readwrite foo.tar.gz)

    foreach(kio_test ${kio_tests})
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${kio_test}.h)
            qt2_wrap_cpp(moc_SOURCES
                SOURCES ${kio_test}.h
                )
        endif()
        add_executable(${kio_test}
            ${kio_test}.cpp
            ${moc_SOURCES}
            )
        target_link_libraries(${kio_test}
            kde2::kio
            )
        add_test(NAME ${kio_test} COMMAND ${kio_test})
        set_tests_properties(${kio_test} PROPERTIES TIMEOUT 5)
        set_tests_properties(${kio_test} PROPERTIES ENVIRONMENT KDE_DEBUG=1)
    endforeach()

    foreach(ksycoca_test ${ksycoca_tests})
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${ksycoca_test}.h)
            qt2_wrap_cpp(moc_SOURCES
                SOURCES ${ksycoca_test}.h
                )
        endif()
        add_executable(${ksycoca_test}
            ${ksycoca_test}.cpp
            ${moc_SOURCES}
            )
        target_link_libraries(${ksycoca_test}
            kde2::ksycoca
            )
        add_test(NAME ${ksycoca_test} COMMAND ${ksycoca_test})
        set_tests_properties(${ksycoca_test} PROPERTIES TIMEOUT 5)
        set_tests_properties(${ksycoca_test} PROPERTIES ENVIRONMENT KDE_DEBUG=1)
    endforeach()
endif(BUILD_TESTS)

if (BUILD_EXAMPLES)
    add_executable(kdirwatchtest ktartest.cpp)
    target_link_libraries(kdirwatchtest kde2::ksycoca)
endif()
