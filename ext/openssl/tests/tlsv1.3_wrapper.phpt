--TEST--
tlsv1.3 stream wrapper
--EXTENSIONS--
openssl
--SKIPIF--
<?php
if (!function_exists("proc_open")) die("skip no proc_open");
?>
--FILE--
<?php
$certFile = __DIR__ . DIRECTORY_SEPARATOR . 'tlsv1.3_wrapper.pem.tmp';

$serverCode = <<<'CODE'
    $flags = STREAM_SERVER_BIND|STREAM_SERVER_LISTEN;
    $ctx = stream_context_create(['ssl' => [
        'local_cert' => '%s',
    ]]);

    $server = stream_socket_server('tlsv1.3://127.0.0.1:0', $errno, $errstr, $flags, $ctx);
    phpt_notify_server_start($server);

    for ($i=0; $i < 3; $i++) {
        @stream_socket_accept($server, 3);
    }
CODE;
$serverCode = sprintf($serverCode, $certFile);

$clientCode = <<<'CODE'
    $flags = STREAM_CLIENT_CONNECT;
    $ctx = stream_context_create(['ssl' => [
        'verify_peer' => false,
        'verify_peer_name' => false,
    ]]);

    $client = stream_socket_client("tlsv1.3://{{ ADDR }}", $errno, $errstr, 3, $flags, $ctx);
    var_dump($client);

    $client = @stream_socket_client("tlsv1.0://{{ ADDR }}", $errno, $errstr, 3, $flags, $ctx);
    var_dump($client);

    $client = @stream_socket_client("tlsv1.2://{{ ADDR }}", $errno, $errstr, 3, $flags, $ctx);
    var_dump($client);
CODE;

include 'CertificateGenerator.inc';
$certificateGenerator = new CertificateGenerator();
$certificateGenerator->saveNewCertAsFileWithKey('tlsv1.3_wrapper', $certFile);

include 'ServerClientTestCase.inc';
ServerClientTestCase::getInstance()->run($clientCode, $serverCode);
?>
--CLEAN--
<?php
@unlink(__DIR__ . DIRECTORY_SEPARATOR . 'tlsv1.3_wrapper.pem.tmp');
?>
--EXPECTF--
resource(%d) of type (stream)
bool(false)
bool(false)
