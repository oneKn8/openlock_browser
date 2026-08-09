find_program(GETTEXT_XGETTEXT_EXECUTABLE xgettext)

set(DOMAIN ${PROJECT_NAME})
set(POT_FILE ${DOMAIN}.pot)

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/LINGUAS LINGUAS
     REGEX "^[^#].*")
string(REGEX MATCHALL "[^ \t]+" LANGS "${LINGUAS}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/POTFILES.in
               ${CMAKE_CURRENT_BINARY_DIR}/POTFILES COPYONLY
)

# Creates the .pot file containing the translations template
add_custom_target(${POT_FILE} ALL
                  COMMENT "Generating translation template"
                  # Extract the translatable messages from the desktop file
                  COMMAND ${GETTEXT_XGETTEXT_EXECUTABLE} -o ${POT_FILE}
                          --from-code=UTF-8
                          --language=Desktop
                          --package-name='${GETTEXT_PACKAGE}'
                          -D ${CMAKE_BINARY_DIR}/data
                          ${DESKTOP_FILE}.in
                  COMMAND ${GETTEXT_XGETTEXT_EXECUTABLE} -o ${POT_FILE}
                          --from-code=UTF-8
                          --c++
                          --qt
                          --add-comments=TRANSLATORS
                          --keyword=tr
                          --keyword=tr:1,2
                          --keyword=N_
                          --keyword=_
                          --package-name='${PROJECT_NAME}'
                          -D ${CMAKE_SOURCE_DIR}
                          -D ${CMAKE_CURRENT_SOURCE_DIR}
                          -D ${CMAKE_CURRENT_BINARY_DIR}
                          --join-existing
                          --files-from=${CMAKE_CURRENT_BINARY_DIR}/POTFILES)

foreach(LANG ${LANGS})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${LANG}.po
                   ${CMAKE_CURRENT_BINARY_DIR}/${LANG}.po
                   COPYONLY)
endforeach(LANG)
gettext_process_pot_file(${POT_FILE} ALL
                         INSTALL_DESTINATION ${CMAKE_INSTALL_LOCALEDIR}
                         LANGUAGES ${LANGS})
