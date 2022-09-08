#include <uvgrtp/lib.hh>

#include <climits>
#include <iostream>
#include <cstring>

/* Encryption is also supported by uvgRTP. Encryption is facilitated
 * by Secure RTP (SRTP) protocol. In order to use SRTP, the encryption
 * context must be exchanged in some way. uvgRTP offers two main methods
 * for exchanging the encryption contexts.
 *
 * This example presents the user implemented encryption key
 * negotiation. In this scenario the encryption keys are exchanged
 * by the application and handed over to uvgRTP. */


// network parameters of example
constexpr char SENDER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t LOCAL_PORT = 8888;

constexpr char RECEIVER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8890;

// encryption parameters of example
enum Key_length{SRTP_128 = 128, SRTP_196 = 196, SRTP_256 = 256};
constexpr Key_length KEY_SIZE = SRTP_256;
constexpr int KEY_SIZE_BYTES = KEY_SIZE/8;
constexpr int SALT_SIZE = 112;
constexpr int SALT_SIZE_BYTES = SALT_SIZE/8;

// demonstration parameters
constexpr auto EXAMPLE_DURATION = std::chrono::seconds(5);
constexpr int FRAME_RATE = 30; // fps
constexpr int SEND_TEST_PACKETS = (EXAMPLE_DURATION.count() - 1)*FRAME_RATE;
constexpr int PACKET_INTERVAL_MS = 1000/FRAME_RATE;
constexpr int RECEIVER_WAIT_TIME_MS = 100;

void process_frame(uvgrtp::frame::rtp_frame *frame);
void receive_func(uint8_t key[KEY_SIZE_BYTES], uint8_t salt[SALT_SIZE_BYTES]);
void wait_until_next_frame(std::chrono::steady_clock::time_point& start, int frame_index);

int main(void)
{
    uvgrtp::context ctx;

    // first we check if crypto has been included before attempting to use it
    if (!ctx.crypto_enabled())
    {
        std::cerr << "Cannot run SRTP example if crypto is not included in uvgRTP!"
                  << std::endl;
        return EXIT_FAILURE;
    }

    /* Key and salt for the SRTP session of sender and receiver
     *
     * NOTE: uvgRTP supports 128, 196 and 256 bit keys and 112 bit salts */
    uint8_t key[KEY_SIZE_BYTES]   = { 0 };
    uint8_t salt[SALT_SIZE_BYTES] = { 0 };

    // initialize SRTP key and salt with dummy values
    for (int i = 0; i < KEY_SIZE_BYTES; ++i)
        key[i] = i;

    for (int i = 0; i < SALT_SIZE_BYTES; ++i)
        salt[i] = i * 2;

    std::cout << "Starting uvgRTP SRTP user provided encryption key example. Using key:"
              << key << " and salt: " << salt << std::endl;

    // Create separate thread for the receiver
    std::thread receiver(receive_func, key, salt);

    // Enable SRTP and let user manage the keys
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER | RCE_SRTP_KEYSIZE_256;

    uvgrtp::session *sender_session = ctx.create_session(RECEIVER_ADDRESS);
    uvgrtp::media_stream *send = sender_session->create_stream(LOCAL_PORT, REMOTE_PORT,
                                                               RTP_FORMAT_GENERIC, flags);

    if (send)
    {
        /* When using user managed keys, before anything else can be done
         * the add_srtp_ctx() must be called with the user SRTP key and salt.
         *
         * All calls to "send" that try to modify and or/use the newly
         * created media stream before calling add_srtp_ctx() will fail
         * if SRTP is enabled */
        send->add_srtp_ctx(key, salt);

        // All media is now encrypted/decrypted automatically
        char* message = (char*)"Hello, world!";
        size_t msg_len = strlen(message);

        auto start = std::chrono::steady_clock::now();
        for (unsigned int i = 0; i < SEND_TEST_PACKETS; ++i)
        {
            if ((i+1)%10  == 0 || i == 0) // print every 10 frames and first
                std::cout << "Sending frame # " << i + 1 << '/' << SEND_TEST_PACKETS << std::endl;

            uint8_t* message_data = new uint8_t[msg_len];
            memcpy(message_data, message, msg_len);

            if (send->push_frame((uint8_t *)message_data, msg_len, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cerr << "Failed to send frame" << std::endl;
            }

            // wait until it is time to send the next frame. Included only for
            // demostration purposes since you can use uvgRTP to send packets as fast as desired
            wait_until_next_frame(start, i);
        }

        sender_session->destroy_stream(send);
    }
    else
    {
      std::cerr << "Failed to create SRTP sender" << std::endl;
    }

    if (receiver.joinable())
    {
      receiver.join();
    }

    if (sender_session)
        ctx.destroy_session(sender_session);

    return EXIT_SUCCESS;
}

void receive_func(uint8_t key[KEY_SIZE_BYTES], uint8_t salt[SALT_SIZE_BYTES])
{
    /* See sending.cc for more details */
    uvgrtp::context ctx;
    uvgrtp::session *receiver_session = ctx.create_session(SENDER_ADDRESS);

    /* Enable SRTP and let user manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;

    /* With user-managed keys, you have the option to use 192- and 256-bit keys.
     *
     * If 192- or 256-bit key size is specified in the flags, add_srtp_ctx() expects
     * the key parameter to be 24 or 32 bytes long, respectively. */
    flags |= RCE_SRTP_KEYSIZE_256;

    /* See sending.cc for more details about create_stream() */
    uvgrtp::media_stream *recv = receiver_session->create_stream(REMOTE_PORT, LOCAL_PORT,
                                                                 RTP_FORMAT_GENERIC, flags);

    // Receive frames by pulling for EXAMPLE_DURATION milliseconds
    if (recv)
    {
        /* Before anything else can be done,
         * add_srtp_ctx() must be called with the SRTP key and salt.
         *
         * All calls to "recv" that try to modify and or/use the newly
         * created media stream before calling add_srtp_ctx() will fail */
        recv->add_srtp_ctx(key, salt);

        std::cout << "Start receiving frames for " << EXAMPLE_DURATION.count() << " s" << std::endl;
        auto start = std::chrono::steady_clock::now();

        uvgrtp::frame::rtp_frame *frame = nullptr;
        while (std::chrono::steady_clock::now() - start < EXAMPLE_DURATION)
        {
            /* You can specify a timeout for the operation and if the a frame is not received
             * within that time limit, pull_frame() returns a nullptr
             *
             * The parameter tells how long time a frame is waited in milliseconds */

            frame = recv->pull_frame(RECEIVER_WAIT_TIME_MS);
            if (frame)
            {
                process_frame(frame);
            }
        }

        receiver_session->destroy_stream(recv);
    }

    if (receiver_session)
    {
        ctx.destroy_session(receiver_session);
    }
}

void process_frame(uvgrtp::frame::rtp_frame *frame)
{
    std::string payload = std::string((char*)frame->payload, frame->payload_len);

    std::cout << "Received SRTP frame. Payload: " << payload << std::endl;

    /* When we receive a frame, the ownership of the frame belongs to us and
     * when we're done with it, we need to deallocate the frame */
    (void)uvgrtp::frame::dealloc_frame(frame);
}

void wait_until_next_frame(std::chrono::steady_clock::time_point &start, int frame_index)
{
  // wait until it is time to send the next frame. Simulates a steady sending pace
  // and included only for demostration purposes since you can use uvgRTP to send
  // packets as fast as desired
  auto time_since_start = std::chrono::steady_clock::now() - start;
  auto next_frame_time = (frame_index + 1)*std::chrono::milliseconds(PACKET_INTERVAL_MS);
  if (next_frame_time > time_since_start)
  {
      std::this_thread::sleep_for(next_frame_time - time_since_start);
  }
}
