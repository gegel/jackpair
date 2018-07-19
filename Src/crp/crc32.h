void crc32_init( void );
void crc32_update( unsigned char *blk_adr, unsigned long blk_len );
unsigned long crc32_value( void );

unsigned long crc32( unsigned char *blk_adr, unsigned long blk_len );
