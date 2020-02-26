/*
 * bpchat.c
 * Andrew Jenkins <andrew.jenkins@colorado.edu>
 * Reads lines from stdin and sends those in bundles.
 * Receives bundles and writes them to stdout.
 */

#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <bp.h>
#include <getopt.h>

static BpSAP                sap;
static Sdr                  sdr;
static pthread_mutex_t      sdrmutex = PTHREAD_MUTEX_INITIALIZER;
static char                 *destEid = NULL;
static char                 *ownEid = NULL;
static BpCustodySwitch      custodySwitch = NoCustodyRequested;
static int                  running = 1;
static int		    					controlZco;
static unsigned char 				raw_mode;

const char usage[] =
"Usage: bpchat.c <source EID> <dest EID> [ct]\n\n"
"Reads lines from stdin and sends these lines in bundles.\n"
"Receives bundles and writes them to stdout.\n"
"If \"ct\" is specified, sent bundles have the custody transfer flag set\n";

static pthread_t    sendLinesThread;
static void *       sendLines(void *args)
{
	Object          bundleZco, bundlePayload;
	Object          newBundle;   /* We never use but bp_send requires it. */
	int             lineLength = 0;
	char            lineBuffer[1024];

	while(running)
	{
		if( (lineLength = read(STDIN_FILENO, lineBuffer, sizeof(lineBuffer)-1)) <= 0 )
		{
			fprintf(stderr, "EOF\n");
			running = 0;
			bp_interrupt(sap);
			break;
		}

		else if( raw_mode == 0 )
		{
			lineBuffer[lineLength] = '\0';
			lineLength = strlen(lineBuffer);
		}

		/* Wrap the linebuffer in a bundle payload. */
		if(pthread_mutex_lock(&sdrmutex) != 0)
		{
			putErrmsg("Couldn't take sdr mutex.", NULL);
			break;
		}

		oK(sdr_begin_xn(sdr));
		bundlePayload = sdr_malloc(sdr, lineLength);

		if(bundlePayload)
		{
			sdr_write(sdr, bundlePayload, lineBuffer, lineLength);
		}

		if(sdr_end_xn(sdr) < 0)
		{
			pthread_mutex_unlock(&sdrmutex);
			bp_close(sap);
			putErrmsg("No space for bpchat payload.", NULL);
			break;
		}

		bundleZco = ionCreateZco(ZcoSdrSource, bundlePayload, 0,
				lineLength, &controlZco);

		if(bundleZco == 0)
		{
			pthread_mutex_unlock(&sdrmutex);
			bp_close(sap);
			putErrmsg("bpchat can't create bundle ZCO.", NULL);
			break;
		}

		pthread_mutex_unlock(&sdrmutex);

		/* Send the bundle payload. */
		if(bp_send(sap, destEid, NULL, 86400, BP_STD_PRIORITY,
				custodySwitch, 0, 0, NULL, bundleZco,
				&newBundle) <= 0)
		{
			putErrmsg("bpchat can't send bundle.", NULL);
			break;
		}

	}
	return NULL;
}

static pthread_t    recvBundlesThread;
static void *       recvBundles(void *args)
{
	BpDelivery      dlv;
	ZcoReader       reader;
	char            buffer[1024];
	int             bundleLenRemaining;
	int             rc;
	int             bytesToRead;

	while(running) {
		if(bp_receive(sap, &dlv, BP_BLOCKING) < 0)
		{
			putErrmsg("bpchat bundle reception failed.", NULL);
			break;
		}

		if(dlv.result == BpReceptionInterrupted || dlv.adu == 0) {
			bp_release_delivery(&dlv, 1);
			continue;
		}

		if(pthread_mutex_lock(&sdrmutex) != 0)
		{
			putErrmsg("Couldn't take sdr mutex.", NULL);
			break;
		}

		oK(sdr_begin_xn(sdr));
		bundleLenRemaining = zco_source_data_length(sdr, dlv.adu);
		zco_start_receiving(dlv.adu, &reader);
		while(bundleLenRemaining > 0) {
			bytesToRead = MIN(bundleLenRemaining, sizeof(buffer)-1);
			rc = zco_receive_source(sdr, &reader, bytesToRead, buffer);
			if(rc < 0) break;
			bundleLenRemaining -= rc;
			if( raw_mode == 1 )
			{
				write(STDOUT_FILENO, buffer, rc);
			}
			else
			{
				printf("%.*s", rc, buffer);
				fflush(stdout);
			}
		}

		if (sdr_end_xn(sdr) < 0)
		{
			running = 0;
		}

		pthread_mutex_unlock(&sdrmutex);
		bp_release_delivery(&dlv, 1);
	}
	return NULL;
}

void handleQuit(int sig)
{
	running = 0;
	pthread_end(sendLinesThread);
	bp_interrupt(sap);
	ionCancelZcoSpaceRequest(&controlZco);
}

int main(int argc, char **argv)
{
	unsigned char ctArg = 0;

	int option_index, opt;

	static struct option long_options[] =
	{
		{"in", required_argument, 0, 'i'},
		{"out", required_argument, 0, 'o'},
		{"ct", no_argument, 0, 'c'},
		{"raw", no_argument, 0, 'r'},
		{"text", no_argument, 0, 't'},
		{"help", no_argument, 0, 'h'},
		{0,0,0,0}
	};

	while( (opt = getopt_long(argc, argv, "hi:o:crt", long_options, &option_index)) != -1 )
	{
		switch( opt )
		{
			case 'i':
				ownEid = optarg;
				break;

			case 'o':
				destEid = optarg;
				break;

			case 'c':
				ctArg = 1;
				break;

			case 'r':
				raw_mode = 1;
				break;

			case 't':
				raw_mode = 0;
				break;

			case 'h':
				fprintf(stderr, usage);
				return 1;

			case '?':
				break;

			default:
				printf("????? lol");
		}
	}

	if(ctArg) {
		custodySwitch = SourceCustodyRequired;
	}

	if(bp_attach() < 0) {
		putErrmsg("Can't bp_attach()", NULL);
		exit(1);
	}

	if(bp_open(ownEid, &sap) < 0)
	{
		putErrmsg("Can't open own endpoint.", ownEid);
		exit(1);
	}

	sdr = bp_get_sdr();

	signal(SIGINT, handleQuit);

	/* Start receiver thread and sender thread. */
	if(pthread_begin(&sendLinesThread, NULL, sendLines, NULL) < 0)
	{
		putErrmsg("Can't make sendLines thread.", NULL);
		bp_interrupt(sap);
		exit(1);
	}

	if(pthread_begin(&recvBundlesThread, NULL, recvBundles, NULL) < 0) {
		putErrmsg("Can't make recvBundles thread.", NULL);
		bp_interrupt(sap);
		exit(1);
	}

	pthread_join(sendLinesThread, NULL);
	pthread_join(recvBundlesThread, NULL);

	bp_close(sap);
	bp_detach();

	return 0;
}
