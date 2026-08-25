# Derives the Mandrill version from git tags, so that it can never fall behind the code it describes.
#
# The most recent tag of the form v<major>.<minor>.<patch> gives the three version numbers, and git describe adds
# how many commits have been made since that tag and which commit the tree is at. A build made straight off a
# release tag reports "1.2.0", one made eleven commits later reports "1.2.0-11-g1a2b3c4", and one with uncommitted
# changes on top gets a "-dirty" suffix. This means the only thing that has to be maintained by hand is the tag
# itself, which is created when a release is actually cut:
#
#     git tag -a v1.2.0 -m "Mandrill 1.2.0"
#     git push origin v1.2.0
#
# When there is no git history to read, which is what happens when Mandrill is consumed as a source archive rather
# than as a repository, MANDRILL_VERSION_FALLBACK is used instead.

# Committing changes what git describe reports, so let CMake reconfigure when HEAD or the current branch moves.
# Without this an incremental build keeps reporting the version from whenever CMake last ran.
function(mandrill_watch_git_head REPO_DIR)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --absolute-git-dir
        WORKING_DIRECTORY "${REPO_DIR}"
        OUTPUT_VARIABLE gitDir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE result
    )

    if (NOT result EQUAL 0)
        return()
    endif()

    # packed-refs covers tags that have been packed away, the symbolic ref covers ordinary commits on a branch
    set(watched "${gitDir}/HEAD" "${gitDir}/packed-refs")

    execute_process(
        COMMAND ${GIT_EXECUTABLE} symbolic-ref -q HEAD
        WORKING_DIRECTORY "${REPO_DIR}"
        OUTPUT_VARIABLE ref
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if (ref)
        list(APPEND watched "${gitDir}/${ref}")
    endif()

    foreach (file IN LISTS watched)
        # A dependency on a file that does not exist would reconfigure on every build
        if (EXISTS "${file}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${file}")
        endif()
    endforeach()
endfunction()

# Read the version out of the repository in REPO_DIR. OUT_VERSION gets the three numbers that CMake needs, and
# OUT_VERSION_STRING gets the full description meant for humans to read.
function(mandrill_version REPO_DIR OUT_VERSION OUT_VERSION_STRING)
    set(version "${MANDRILL_VERSION_FALLBACK}")
    set(versionString "${MANDRILL_VERSION_FALLBACK}-unknown")

    find_program(GIT_EXECUTABLE NAMES git)

    if (GIT_EXECUTABLE)
        # Make sure the tags about to be read are Mandrill's own, and not those of a repository that Mandrill was
        # copied into. A submodule has a repository of its own, so that case reads the right tags.
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --show-toplevel
            WORKING_DIRECTORY "${REPO_DIR}"
            OUTPUT_VARIABLE toplevel
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE result
        )

        get_filename_component(toplevel "${toplevel}" REALPATH)
        get_filename_component(repoDir "${REPO_DIR}" REALPATH)

        if (result EQUAL 0 AND toplevel STREQUAL repoDir)
            # Refresh the stat cache the index keeps, otherwise the check that describe makes for local changes
            # reports files as modified just because their timestamps moved, and every build looks dirty. This only
            # rewrites the cache, never any content, and failing to do so is not worth reporting.
            execute_process(
                COMMAND ${GIT_EXECUTABLE} update-index -q --refresh
                WORKING_DIRECTORY "${REPO_DIR}"
                OUTPUT_QUIET
                ERROR_QUIET
            )

            execute_process(
                COMMAND ${GIT_EXECUTABLE} describe --tags --long --dirty --match "v[0-9]*"
                WORKING_DIRECTORY "${REPO_DIR}"
                OUTPUT_VARIABLE describe
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE result
            )

            if (result EQUAL 0 AND describe MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)-([0-9]+)-g([0-9a-f]+)")
                set(version "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
                set(commitsSinceTag "${CMAKE_MATCH_4}")

                # A build straight off a tag is simply that release, anything later carries how far it has drifted
                if (commitsSinceTag EQUAL 0)
                    set(versionString "${version}")
                else()
                    set(versionString "${version}-${commitsSinceTag}-g${CMAKE_MATCH_5}")
                endif()

                if (describe MATCHES "-dirty$")
                    set(versionString "${versionString}-dirty")
                endif()

                mandrill_watch_git_head("${REPO_DIR}")
            else()
                message(WARNING
                    "No Mandrill version tag was found, using ${MANDRILL_VERSION_FALLBACK} instead. Tag a release "
                    "with `git tag -a v1.0.0 -m \"Mandrill 1.0.0\"`, or fetch the tags if this is a shallow clone.")
            endif()
        endif()
    endif()

    set(${OUT_VERSION} "${version}" PARENT_SCOPE)
    set(${OUT_VERSION_STRING} "${versionString}" PARENT_SCOPE)
endfunction()
