#include <netdb.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "transport.h"

#include "test.h"

static void
transportCreateUdpReturnsValidPtrInHappyPath(void** state)
{
    transport_t* t = transportCreateUdp("127.0.0.1", "8126");
    assert_non_null(t);
    transportDestroy(&t);
}

static void
transportCreateUdpReturnsNullPtrForInvalidHost(void** state)
{
    // This is a valid test, but it hangs for as long as 30s.
    // (It depends on dns more than a unit test should.)
/*
    t = transportCreateUdp("-tota11y bogus hostname", "666");
    assert_null(t);
*/

    transport_t* t;
    t = transportCreateUdp(NULL, "8128");
    assert_null(t);
}

static void
transportCreateUdpReturnsNullPtrForInvalidPort(void** state)
{
    transport_t* t;
    t = transportCreateUdp("127.0.0.1", "mom's apple pie recipe");
    assert_null(t);
    transportDestroy(&t);

    t = transportCreateUdp("127.0.0.1", NULL);
    assert_null(t);
    transportDestroy(&t);
}

static void
transportCreateUdpHandlesGoodHostArguments(void** state)
{
    const char* host_values[] = {
           "localhost", "www.google.com",
           "127.0.0.1", "0.0.0.0", "8.8.4.4",
           "::1", "::ffff:127.0.0.1", "::", 
           // These will work only if machine supports ipv6
           //"2001:4860:4860::8844",
           //"ipv6.google.com",
    };

    int i;
    for (i=0; i<sizeof(host_values)/sizeof(host_values[0]); i++) {
        transport_t* t = transportCreateUdp(host_values[i], "1234");
        assert_non_null(t);
        transportDestroy(&t);
    }
}

static void
transportCreateFileReturnsValidPtrInHappyPath(void** state)
{
    const char* path = "/tmp/myscope.log";
    transport_t* t = transportCreateFile(path);
    assert_non_null(t);
    transportDestroy(&t);
    assert_null(t);
    if (unlink(path))
        fail_msg("Couldn't delete test file %s", path);
}

static void
transportCreateFileCreatesFileWithRWPermissionsForAll(void** state)
{
    const char* path = "/tmp/myscope.log";
    transport_t* t = transportCreateFile(path);
    assert_non_null(t);
    transportDestroy(&t);

    // test permissions are 0666
    struct stat buf;
    if (stat(path, &buf))
        fail_msg("Couldn't test permissions for file %s", path);
    assert_true((buf.st_mode & 0777) == 0666);

    // Clean up
    if (unlink(path))
        fail_msg("Couldn't delete test file %s", path);
}

static void
transportCreateFileReturnsNullForInvalidPath(void** state)
{
    transport_t* t;
    t = transportCreateFile(NULL);
    assert_null(t);
    t = transportCreateFile("");
    assert_null(t);
}

static void
transportCreateFileCreatesDirectoriesAsNeeded(void** state)
{
    // This should work in my opinion, but doesn't right now
    skip();

    transport_t* t = transportCreateFile("/var/log/out/directory/path/here.log");
    assert_non_null(t);
}

static void
transportCreateUnixReturnsValidPtrInHappyPath(void** state)
{
    transport_t* t = transportCreateUnix("/my/favorite/path");
    assert_non_null(t);
    transportDestroy(&t);
    assert_null(t);

}

static void
transportCreateUnixReturnsNullForInvalidPath(void** state)
{
    transport_t* t = transportCreateUnix(NULL);
    assert_null(t);
}


static void
transportCreateSyslogReturnsValidPtrInHappyPath(void** state)
{
    transport_t* t = transportCreateSyslog();
    assert_non_null(t);
    transportDestroy(&t);
    assert_null(t);

}

static void
transportCreateShmReturnsValidPtrInHappyPath(void** state)
{
    transport_t* t = transportCreateShm();
    assert_non_null(t);
    transportDestroy(&t);
    assert_null(t);
}

static void
transportDestroyNullTransportDoesNothing(void** state)
{
    transportDestroy(NULL);
    transport_t* t = NULL;
    transportDestroy(&t);
    // Implicitly shows that calling transportDestroy with NULL is harmless
}

static void
transportSendForNullTransportDoesNothing(void** state)
{
    const char* msg = "Hey, this is cool!\n";
    assert_int_equal(transportSend(NULL, msg), -1);
}

static void
transportSendForNullMessageDoesNothing(void** state)
{
    const char* path = "/tmp/path";
    transport_t* t = transportCreateFile(path);
    assert_non_null(t);
    assert_int_equal(transportSend(t, NULL), -1);
    transportDestroy(&t);
    if (unlink(path))
        fail_msg("Couldn't delete test file %s", path);
}

static void
transportSendForUnimplementedTransportTypesIsHarmless(void** state)
{
    transport_t* t;
    t = transportCreateUnix("/my/favorite/path");
    transportSend(t, "blah");
    transportDestroy(&t);

    t = transportCreateSyslog();
    transportSend(t, "blah");
    transportDestroy(&t);

    t = transportCreateShm();
    transportSend(t, "blah");
    transportDestroy(&t);
}

static void
transportSendForUdpTransmitsMsg(void** state)
{
    const char* hostname = "127.0.0.1";
    const char* portname = "8126";
    struct addrinfo hints = {0};
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_DGRAM;
    hints.ai_flags=AI_PASSIVE|AI_ADDRCONFIG;
    struct addrinfo* res = NULL;
    if (getaddrinfo(hostname, portname, &hints, &res)) {
        fail_msg("Couldn't create address for socket");
    }
    int sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sd == -1) {
        fail_msg("Couldn't create socket");
    }
    if (bind(sd, res->ai_addr, res->ai_addrlen) == -1) {
        fail_msg("Couldn't bind socket");
    }
    freeaddrinfo(res);

    transport_t* t = transportCreateUdp(hostname, portname);
    assert_non_null(t);
    const char msg[] = "This is the payload message to transfer.\n";
    char buf[sizeof(msg)] = {0};  // Has room for a null at the end
    assert_int_equal(transportSend(t, msg), 0);

    struct sockaddr_storage from = {0};
    socklen_t len = sizeof(from);
    int byteCount=0;
    if ((byteCount = recvfrom(sd, buf, sizeof(buf), 0, (struct sockaddr*)&from, &len)) != strlen(msg)) {
        fail_msg("Couldn't recvfrom");
    }
    assert_string_equal(msg, buf);

    transportDestroy(&t);

    close(sd);
}

static void
transportSendForFileWritesToFile(void** state)
{
    const char* path = "/tmp/mypath";
    transport_t* t = transportCreateFile(path);
    assert_non_null(t);

    // open the file with the position at the end
    FILE* f = fopen(path, "r+");
    if (!f)
        fail_msg("Couldn't open file %s", path);
    if (fseek(f, 0, SEEK_END))
        fail_msg("Couldn't seek to end of file %s", path);

    // Since we're at the end, nothing should be there
    char buf[1024];
    const int maxReadSize = sizeof(buf)-1;
    size_t bytesRead = fread(buf, 1, maxReadSize, f);
    assert_int_equal(bytesRead, 0);
    assert_true(feof(f));
    assert_false(ferror(f));
    clearerr(f);

    const char msg[] = "This is the payload message to transfer.\n";
    assert_int_equal(transportSend(t, msg), 0);

    // Test that after the transportSend, that the msg got there.
    bytesRead = fread(buf, 1, maxReadSize, f);
    // Provide the null ourselves.  Safe because of maxReadSize
    buf[bytesRead] = '\0';

    assert_int_equal(bytesRead, strlen(msg));
    assert_true(feof(f));
    assert_false(ferror(f));
    assert_string_equal(msg, buf);

    if (fclose(f)) fail_msg("Couldn't close file %s", path);

    transportDestroy(&t);

    if (unlink(path))
        fail_msg("Couldn't delete test file %s", path);
}

int
main (int argc, char* argv[])
{
    printf("running %s\n", argv[0]);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(transportCreateUdpReturnsValidPtrInHappyPath),
        cmocka_unit_test(transportCreateUdpReturnsNullPtrForInvalidHost),
        cmocka_unit_test(transportCreateUdpReturnsNullPtrForInvalidPort),
        cmocka_unit_test(transportCreateUdpHandlesGoodHostArguments),
        cmocka_unit_test(transportCreateFileReturnsValidPtrInHappyPath),
        cmocka_unit_test(transportCreateFileCreatesFileWithRWPermissionsForAll),
        cmocka_unit_test(transportCreateFileReturnsNullForInvalidPath),
        cmocka_unit_test(transportCreateFileCreatesDirectoriesAsNeeded),
        cmocka_unit_test(transportCreateUnixReturnsValidPtrInHappyPath),
        cmocka_unit_test(transportCreateUnixReturnsNullForInvalidPath),
        cmocka_unit_test(transportCreateSyslogReturnsValidPtrInHappyPath),
        cmocka_unit_test(transportCreateShmReturnsValidPtrInHappyPath),
        cmocka_unit_test(transportDestroyNullTransportDoesNothing),
        cmocka_unit_test(transportSendForNullTransportDoesNothing),
        cmocka_unit_test(transportSendForNullMessageDoesNothing),
        cmocka_unit_test(transportSendForUnimplementedTransportTypesIsHarmless),
        cmocka_unit_test(transportSendForUdpTransmitsMsg),
        cmocka_unit_test(transportSendForFileWritesToFile),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
