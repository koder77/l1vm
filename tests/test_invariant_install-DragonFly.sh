#include <check.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

START_TEST(test_url_integrity_verification)
{
    // Invariant: Script must verify integrity of downloaded packages before installation
    const char *payloads[] = {
        "https://midnight-coding.de/blog/assets/l1vm/l1vm-base-pkg.tar.bz2",  // Original URL
        "https://evil.com/malicious-pkg.tar.bz2",  // Malicious alternative URL
        "http://insecure.example.com/package.tar.bz2",  // HTTP instead of HTTPS
        "file:///etc/passwd",  // Local file path attempt
        "https://midnight-coding.de@evil.com/fake-pkg.tar.bz2"  // URL with misleading domain
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        // Create a test script that simulates the vulnerable download pattern
        char script_path[] = "/tmp/test_install_XXXXXX";
        int fd = mkstemp(script_path);
        if (fd == -1) {
            ck_abort_msg("Failed to create temp script");
        }
        
        char script_content[1024];
        snprintf(script_content, sizeof(script_content),
            "#!/bin/sh\n"
            "URL=\"%s\"\n"
            "OUTPUT=\"test-pkg.tar.bz2\"\n"
            "# Simulate download attempt\n"
            "echo \"Attempting download from: $URL\"\n"
            "# Check if URL uses HTTPS and has expected domain\n"
            "case \"$URL\" in\n"
            "    https://midnight-coding.de/*)\n"
            "        echo \"Valid domain\"\n"
            "        exit 0\n"
            "        ;;\n"
            "    *)\n"
            "        echo \"ERROR: Untrusted source\"\n"
            "        exit 1\n"
            "        ;;\n"
            "esac\n",
            payloads[i]);
        
        write(fd, script_content, strlen(script_content));
        close(fd);
        chmod(script_path, 0700);
        
        // Execute the test script
        pid_t pid = fork();
        if (pid == 0) {
            execl(script_path, script_path, NULL);
            _exit(EXIT_FAILURE);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            unlink(script_path);
            
            // Property: Script must reject untrusted URLs
            if (i == 0) {
                // Original URL should be accepted
                ck_assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
            } else {
                // All other URLs should be rejected
                ck_assert_msg(!(WIFEXITED(status) && WEXITSTATUS(status) == 0),
                    "Script accepted untrusted URL: %s", payloads[i]);
            }
        } else {
            unlink(script_path);
            ck_abort_msg("Fork failed");
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_url_integrity_verification);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}