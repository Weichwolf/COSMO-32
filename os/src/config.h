// COSMO-32 Configuration
// Configurable values (could be overridden by build system / TOML)

#ifndef CONFIG_H
#define CONFIG_H

//----------------------------------------------------------------------
// CPU
//----------------------------------------------------------------------

#ifndef CPU_CLOCK_HZ
#define CPU_CLOCK_HZ    144000000   // 144 MHz
#endif

//----------------------------------------------------------------------
// Network
//----------------------------------------------------------------------

// IP addresses (little-endian for direct use)
#ifndef OUR_IP
#define OUR_IP          0x0200000A  // 10.0.0.2
#endif

#ifndef SERVER_IP
#define SERVER_IP       0x0100000A  // 10.0.0.1
#endif

#ifndef SUBNET_MASK
#define SUBNET_MASK     0x00FFFFFF  // 255.255.255.0
#endif

// MAC address
#ifndef OUR_MAC_0
#define OUR_MAC_0       0x02        // Locally administered
#endif
#ifndef OUR_MAC_1
#define OUR_MAC_1       0x00
#endif
#ifndef OUR_MAC_2
#define OUR_MAC_2       0x00
#endif
#ifndef OUR_MAC_3
#define OUR_MAC_3       0x00
#endif
#ifndef OUR_MAC_4
#define OUR_MAC_4       0x00
#endif
#ifndef OUR_MAC_5
#define OUR_MAC_5       0x02
#endif

// Ports
#ifndef TFTP_PORT
#define TFTP_PORT       69
#endif

#ifndef CLIENT_PORT
#define CLIENT_PORT     1234
#endif

//----------------------------------------------------------------------
// Display
//----------------------------------------------------------------------

#ifndef DEFAULT_DISPLAY_MODE
#define DEFAULT_DISPLAY_MODE    0   // 640x400x4bpp
#endif

//----------------------------------------------------------------------
// Audio
//----------------------------------------------------------------------

#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE   22050
#endif

#ifndef AUDIO_BUFFER_SIZE
#define AUDIO_BUFFER_SIZE   2048    // Samples per channel
#endif

//----------------------------------------------------------------------
// BASIC Interpreter
//----------------------------------------------------------------------

#ifndef BASIC_MAX_LINES
#define BASIC_MAX_LINES     128
#endif

#ifndef BASIC_MAX_LINE_LEN
#define BASIC_MAX_LINE_LEN  80
#endif

#ifndef BASIC_MAX_STACK
#define BASIC_MAX_STACK     16
#endif

#ifndef BASIC_MAX_FOR_DEPTH
#define BASIC_MAX_FOR_DEPTH 8
#endif

#endif // CONFIG_H
