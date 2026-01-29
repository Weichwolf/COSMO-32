# CGL - COSMO Graphics Layer

Hardware-Abstraktionsschicht für COSMO-32, inspiriert von SDL2.

## Architektur

```
┌─────────────────────────────────┐
│  Apps / Games / Shell           │
├─────────────────────────────────┤
│  GUI Toolkit (später)           │
├─────────────────────────────────┤
│  CGL                            │
├─────────────────────────────────┤
│  Hardware (FB, USART, I2S)      │
└─────────────────────────────────┘
```

## API Entwurf

### Display

```c
int  cgl_init(int width, int height, int bpp);
void cgl_quit(void);

void cgl_clear(uint8_t color);
void cgl_pixel(int x, int y, uint8_t color);
void cgl_rect(int x, int y, int w, int h, uint8_t color);
void cgl_fill_rect(int x, int y, int w, int h, uint8_t color);
void cgl_line(int x1, int y1, int x2, int y2, uint8_t color);
void cgl_blit(int x, int y, const uint8_t *bitmap, int w, int h);
void cgl_text(int x, int y, const char *str, uint8_t color);
void cgl_flip(void);  // VSync
```

### Input

```c
int  cgl_poll_event(cgl_event_t *event);
int  cgl_wait_event(cgl_event_t *event);
int  cgl_get_key(void);  // Blocking

typedef struct {
    uint8_t type;   // CGL_KEYDOWN, CGL_KEYUP, CGL_QUIT
    uint8_t key;    // Keycode
} cgl_event_t;
```

### Audio

```c
void cgl_play_sample(const int16_t *data, int len, int rate);
void cgl_beep(int freq, int duration_ms);
void cgl_stop_audio(void);
```

### Timer

```c
uint32_t cgl_ticks(void);     // ms seit Start
void     cgl_delay(int ms);
```

## Langfristig

- 9P Filesystem als Netzwerk-FS
- GUI-Toolkit auf CGL (Plan 9 / BeOS inspiriert)
- Maus-Support

## Prinzipien

- Einfach vor vollständig
- Hardware austauschbar, Apps portabel
- Kein dynamischer Speicher in CGL selbst
