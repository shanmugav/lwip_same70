set(DEPENDENT_MP_BIN2HEXberkeley_tcp_client_sam_e70_xult_cEM1HXlk "c:/Program Files/Microchip/xc32/v5.10/bin/xc32-bin2hex.exe")
set(DEPENDENT_DEPENDENT_TARGET_ELFberkeley_tcp_client_sam_e70_xult_cEM1HXlk ${CMAKE_CURRENT_LIST_DIR}/../../../../out/berkeley_tcp_client/sam_e70_xult.elf)
set(DEPENDENT_TARGET_DIRberkeley_tcp_client_sam_e70_xult_cEM1HXlk ${CMAKE_CURRENT_LIST_DIR}/../../../../out/berkeley_tcp_client)
set(DEPENDENT_BYPRODUCTSberkeley_tcp_client_sam_e70_xult_cEM1HXlk ${DEPENDENT_TARGET_DIRberkeley_tcp_client_sam_e70_xult_cEM1HXlk}/${sourceFileNameberkeley_tcp_client_sam_e70_xult_cEM1HXlk}.c)
add_custom_command(
    OUTPUT ${DEPENDENT_TARGET_DIRberkeley_tcp_client_sam_e70_xult_cEM1HXlk}/${sourceFileNameberkeley_tcp_client_sam_e70_xult_cEM1HXlk}.c
    COMMAND ${DEPENDENT_MP_BIN2HEXberkeley_tcp_client_sam_e70_xult_cEM1HXlk} --image ${DEPENDENT_DEPENDENT_TARGET_ELFberkeley_tcp_client_sam_e70_xult_cEM1HXlk} --image-generated-c ${sourceFileNameberkeley_tcp_client_sam_e70_xult_cEM1HXlk}.c --image-generated-h ${sourceFileNameberkeley_tcp_client_sam_e70_xult_cEM1HXlk}.h --image-copy-mode ${modeberkeley_tcp_client_sam_e70_xult_cEM1HXlk} --image-offset ${addressberkeley_tcp_client_sam_e70_xult_cEM1HXlk} 
    WORKING_DIRECTORY ${DEPENDENT_TARGET_DIRberkeley_tcp_client_sam_e70_xult_cEM1HXlk}
    DEPENDS ${DEPENDENT_DEPENDENT_TARGET_ELFberkeley_tcp_client_sam_e70_xult_cEM1HXlk})
add_custom_target(
    dependent_produced_source_artifactberkeley_tcp_client_sam_e70_xult_cEM1HXlk 
    DEPENDS ${DEPENDENT_TARGET_DIRberkeley_tcp_client_sam_e70_xult_cEM1HXlk}/${sourceFileNameberkeley_tcp_client_sam_e70_xult_cEM1HXlk}.c
    )
