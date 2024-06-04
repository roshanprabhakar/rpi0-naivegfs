// TODO in the is_lfn section, convert uint16_t utf16 chars into
// uint8_t ascii chars. We need to do this without a library. 
// This should work after that though.
struct dir_record *print_dir_record(
		struct dir_record *dr
	) {
	
	// This dirrec indicates that no more will follow, and holds
	// no local file info.
	if(IS_TERMINAL(*dr)) { return NULL; }

	// Check whether this entry is part of a long file name (LFN)
	int is_lfn = IS_LFN(dr->dir_attr);
	printk("LFN<%d>", is_lfn);
	if(is_lfn) {
		wchar_t c;
		dr++;
		print_dir_record(dr);

#if 0
		for(int j = 0x1; j < 0xb; j += 2) {
			c = (wchar_t)(((uint16_t *)dr)[j/2]);
			if(c == 0) return dr;
			if(c != '\n') putwchar(c);
		}
		for(int j = 0xe; j < 0x1a; j += 2) {
			c = (wchar_t)(((uint16_t *)dr)[j/2]);
			if(c == 0) return dr;
			if(c != '\n') putwchar(c);
		}
		for(int j = 0x1c; j < 0x1e; j += 2) {
			c = (wchar_t)(((uint16_t *)dr)[j/2]);
			if(c == 0) return dr;
			if(c != '\n') putwchar(c);
		}
#endif
	} else {
		for(int c = 0; c < 11; ++c) { 
			if (dr->dir_name[c] == 0) { break; }
			printk("%c", dr->dir_name[c]); 
		}
		printk("\n");
		return dr + 1;
	}

	return dr;
}

