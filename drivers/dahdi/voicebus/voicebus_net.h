#ifdef VOICEBUS_NET_DEBUG
int vb_net_register(struct voicebus *, const char *);
void vb_net_unregister(struct voicebus *);
void vb_net_capture_vbb(struct voicebus *, const void *,
			const int, const u32, const u16);
#else
#define vb_net_register(a, b) do { ; } while (0)
#define vb_net_unregister(a) do { ; } while (0)
#define vb_net_capture_vbb(a, b, c, d, e) do { ; } while (0)
#endif
