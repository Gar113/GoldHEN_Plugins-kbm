#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <string>

#include "graphics.h"
#include "log.h"

Scene2D::Scene2D(int w, int h, int pixelDepth)
{
	this->width = w;
	this->height = h;
	this->depth = pixelDepth;
	
	this->frameBufferSize = this->width * this->height * this->depth;

	/* Anggota ini WAJIB diinisialisasi. Kalau tidak, nilainya sampah,
	   dan DrawPixel bisa menulis ke frameBuffers[angka sampah] sehingga
	   memori acak tertimpa -> layar berkedip dan tampilan berantakan. */
	this->video = -1;
	this->videoMem = NULL;
	this->videoMemSP = 0;
	this->directMemOff = 0;
	this->directMemAllocationSize = 0;
	this->frameBuffers = NULL;
	this->flipQueue = 0;
	this->frameBufferCount = 0;
	this->activeFrameBufferIdx = 0;
}

bool Scene2D::Init(size_t memSize, int numFrameBuffers)
{
	int rc;
	
	this->video = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, 0);
	this->videoMem = NULL;

	if (this->video < 0)
	{
		DEBUGLOG << "Failed to open a video out handle: " << std::string(strerror(errno));
		return false;
	}
	
#ifdef GRAPHICS_USES_FONT
	// Load freetype
	rc = sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL);

	if (rc < 0)
	{
		DEBUGLOG << "Failed to load freetype: " << std::string(strerror(errno));
		return false;
	}

	// Initialize freetype
	rc = FT_Init_FreeType(&this->ftLib);

	if (rc != 0)
	{
		DEBUGLOG << "Failed to initialize freetype: " << std::string(strerror(errno));
		return false;
	}
#endif
	
	if(!initFlipQueue())
	{
		DEBUGLOG << "Failed to initialize flip queue: " << std::string(strerror(errno));
		return false;
	}
	
	if(!allocateVideoMem(memSize, 0x200000))
	{
		DEBUGLOG << "Failed to allocate video memory: " << std::string(strerror(errno));
		return false;
	}
	
	if(!allocateFrameBuffers(numFrameBuffers))
	{
		DEBUGLOG << "Failed to allocate frame buffers: " << std::string(strerror(errno));
		return false;
	}
	
	sceVideoOutSetFlipRate(this->video, 0);
	return true;
}

bool Scene2D::initFlipQueue()
{
	int rc = sceKernelCreateEqueue(&flipQueue, "homebrew flip queue");
	
	if(rc < 0)
		return false;
		
	sceVideoOutAddFlipEvent(flipQueue, this->video, 0);
	return true;
}

bool Scene2D::allocateFrameBuffers(int num)
{
	// Allocate frame buffers array
	this->frameBuffers = new char*[num];
	this->frameBufferCount = num;
	
	// Set the display buffers
	for(int i = 0; i < num; i++)
		this->frameBuffers[i] = this->allocateDisplayMem(frameBufferSize);

	// Set SRGB pixel format
	sceVideoOutSetBufferAttribute(&this->attr, 0x80000000, 1, 0, this->width, this->height, this->width);
	
	// Register the buffers to the video handle
	return (sceVideoOutRegisterBuffers(this->video, 0, (void **)this->frameBuffers, num, &this->attr) == 0);
}

char *Scene2D::allocateDisplayMem(size_t size)
{
	// Essentially just bump allocation
	char *allocatedPtr = (char *)videoMemSP;
	videoMemSP += size;

	return allocatedPtr;
}

bool Scene2D::allocateVideoMem(size_t size, int alignment)
{
	int rc;
	
	// Align the allocation size
	this->directMemAllocationSize = (size + alignment - 1) / alignment * alignment;
	
	// Allocate memory for display buffer
	rc = sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), this->directMemAllocationSize, alignment, 3, &this->directMemOff);
	
	if(rc < 0)
	{
		this->directMemAllocationSize = 0;
		return false;
	}
	
	// Map the direct memory
	rc = sceKernelMapDirectMemory(&this->videoMem, this->directMemAllocationSize, 0x33, 0, this->directMemOff, alignment);
	
	if(rc < 0)
	{
		sceKernelReleaseDirectMemory(this->directMemOff, this->directMemAllocationSize);
		
		this->directMemOff = 0;
		this->directMemAllocationSize = 0;
		
		return false;
	}
	
	// Set the stack pointer to the beginning of the buffer
	this->videoMemSP = (uintptr_t)this->videoMem;
	return true;
}

void Scene2D::deallocateVideoMem()
{
	// Free the direct memory
	sceKernelReleaseDirectMemory(this->directMemOff, this->directMemAllocationSize);
	
	// Zero out meta data
	this->videoMem = 0;
	this->videoMemSP = 0;
	this->directMemOff = 0;
	this->directMemAllocationSize = 0;
	
	// Free the frame buffer array
	delete this->frameBuffers;
	this->frameBuffers = 0;
}

void Scene2D::SetActiveFrameBuffer(int index)
{
	this->activeFrameBufferIdx = index;
}

void Scene2D::SubmitFlip(int frameID)
{
	sceVideoOutSubmitFlip(this->video, this->activeFrameBufferIdx, ORBIS_VIDEO_OUT_FLIP_VSYNC, frameID);
}

void Scene2D::FrameWait(int frameID)
{
	OrbisKernelEvent evt;
	int count;
	
	// If the video handle is not initialized, bail out. This is mostly a failsafe, this should never happen.
	if(this->video < 0)
		return;
		
	for(;;)
	{
		OrbisVideoOutFlipStatus flipStatus;
		
		// Get the flip status and check the arg for the given frame ID
		sceVideoOutGetFlipStatus(video, &flipStatus);
		
		if(flipStatus.flipArg == frameID)
			break;
			
		// Wait on next flip event.
		// Sebelumnya kalau WaitEqueue gagal kita langsung break, jadi VSync
		// tidak pernah benar-benar ditunggu dan framerate jadi liar ->
		// gambar berkedip/robek. Sekarang tetap lanjut memutar loop.
		sceKernelWaitEqueue(this->flipQueue, &evt, 1, &count, 0);
	}
}

void Scene2D::FrameBufferSwap()
{
	// Swap the frame buffer for some perf.
	// Pakai frameBufferCount, bukan angka 2 yang di-hardcode, supaya
	// tetap benar kalau jumlah frame buffer diubah.
	if (this->frameBufferCount > 1)
		this->activeFrameBufferIdx = (this->activeFrameBufferIdx + 1) % this->frameBufferCount;
	else
		this->activeFrameBufferIdx = 0;
}

void Scene2D::FrameBufferClear()
{
	// Clear the screen with a white frame buffer
	Color blank = { 255, 255, 255 };
	FrameBufferFill(blank);
}

#ifdef GRAPHICS_USES_FONT
bool Scene2D::InitFont(FT_Face *face, const char *fontPath, int fontSize)
{
	int rc;
	
	rc = FT_New_Face(this->ftLib, fontPath, 0, face);
	
	if(rc < 0)
		return false;

	rc = FT_Set_Pixel_Sizes(*face, 0, fontSize);
	
	if(rc < 0)
		return false;
	
	return true;
}

/* Tinggi satu baris teks, diambil dari metrik font.
   Dipakai untuk menghitung kotak sorot dan jarak antar baris. */
int Scene2D::TextHeight(FT_Face face)
{
	if (face == NULL)
		return 0;
	return (int)(face->size->metrics.height >> 6);
}

/* Jarak dari ATAS baris ke baseline. */
int Scene2D::TextAscent(FT_Face face)
{
	if (face == NULL)
		return 0;
	return (int)(face->size->metrics.ascender >> 6);
}

/* Tinggi huruf kapital (tanpa ekor bawah seperti g/y/p).
   Ini yang menentukan tinggi kotak sorot supaya pas dengan teks. */
int Scene2D::TextCapHeight(FT_Face face)
{
	if (face == NULL)
		return 0;

	FT_UInt gi = FT_Get_Char_Index(face, 'A');
	if (gi == 0)
		return (int)(face->size->metrics.ascender >> 6);

	if (FT_Load_Glyph(face, gi, FT_LOAD_DEFAULT) != 0)
		return (int)(face->size->metrics.ascender >> 6);

	return face->glyph->bitmap_top;
}
#endif

void Scene2D::FrameBufferFill(Color color)
{
	DrawRectangle(0, 0, this->width, this->height, color);
}

void Scene2D::DrawPixel(int x, int y, Color color)
{
	// Get pixel location based on pitch
	int pixel = (y * this->width) + x;
	
	// Encode to 24-bit color
	uint32_t encodedColor = 0x80000000 + (color.r << 16) + (color.g << 8) + color.b;
	
	// Draw to the frame buffer
	((uint32_t *)this->frameBuffers[this->activeFrameBufferIdx])[pixel] = encodedColor;
}

void Scene2D::DrawRectangle(int x, int y, int w, int h, Color color)
{
	int xPos, yPos;
	
	// Draw row-by-row, column-by-column
	for(yPos = y; yPos < y + h; yPos++)
	{
		for(xPos = x; xPos < x + w; xPos++)
		{
			DrawPixel(xPos, yPos, color);
		}
	}
}

#ifdef GRAPHICS_USES_FONT
void Scene2D::DrawText(char *txt, FT_Face face, int startX, int startY, Color bgColor, Color fgColor)
{
	int rc;
	int xOffset = 0;
	int yOffset = 0;

	/* Kalau font belum siap atau teks kosong, jangan lanjut.
	   face yang NULL dulu dipakai langsung (face->glyph) sehingga
	   app hang. Sekarang app tetap jalan walau font gagal dimuat. */
	if (face == NULL || txt == NULL)
		return;

	/* PENTING: startY adalah koordinat ATAS baris teks, bukan baseline.
	   Dulu startY dipakai langsung sebagai baseline:
	       y = startY + yPos - slot->bitmap_top;
	   sehingga huruf tergambar ~top pixel DI ATAS startY. Akibatnya
	   teks selalu meleset ke atas dari kotak sorotnya dan baris
	   berikutnya saling tumpang tindih - tampilan terlihat berantakan.
	   Sekarang kita hitung baseline dari tinggi font (ascender) supaya
	   teks pas di dalam kotak barisnya. */
	int baseline = startY + (int)(face->size->metrics.ascender >> 6);

	// Get the glyph slot for bitmap and font metrics
	FT_GlyphSlot slot = face->glyph;
	
	// Iterate each character of the text to write to the screen.
	// Panjang dihitung sekali saja (dulu strlen() dipanggil tiap
	// karakter, itu 60x per frame untuk tiap baris teks).
	int len = (int)strlen(txt);
	for(int n = 0; n < len; n++)
	{
		FT_UInt glyph_index;

        // Get the glyph for the ASCII code
        glyph_index = FT_Get_Char_Index(face, txt[n]);

        // Load and render in 8-bit color
        rc = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);

        if (rc)
            continue;

        rc = FT_Render_Glyph(slot, ft_render_mode_normal);

        if (rc)
            continue;

        // If we get a newline, increment the y offset, reset the x offset, and skip to the next character.
        // Tinggi baris diambil dari metrik font, bukan slot->bitmap.width
        // (nilai itu lebar glyph, bukan tinggi baris, sehingga jarak
        // antar baris jadi kacau).
        if (txt[n] == '\n')
        {
            xOffset = 0;
            yOffset += (int)(face->size->metrics.height >> 6);

            continue;
        }

        // Parse and write the bitmap to the frame buffer
        for (int yPos = 0; yPos < slot->bitmap.rows; yPos++)
        {
            for (int xPos = 0; xPos < slot->bitmap.width; xPos++)
            {
                // Decode the 8-bit bitmap.
                // PENTING: buffer bitmap itu byte tak bertanda. Kalau
                // dibaca sebagai 'char' yang bertanda, nilai di atas 127
                // jadi negatif dan perhitungan warna rusak -> teks kotor.
                unsigned char pixel = (unsigned char)slot->bitmap.buffer[(yPos * slot->bitmap.width) + xPos];

                // Get new pixel coordinates to account for the character position and baseline, as well as newlines
                int x = startX + xPos + xOffset + slot->bitmap_left;
                int y = startY + yPos + yOffset - slot->bitmap_top;

                // Interpolasi antara warna latar dan warna depan memakai
                // nilai alpha dari bitmap. Ini yang membuat tepi huruf
                // halus (anti-aliasing).
                // Versi lama hanya mengalikan warna depan dengan alpha tanpa
                // mencampur latar, sehingga pixel transparan jadi gelap/hitam
                // dan teks tampak bergerigi/kotor.
                uint8_t r = bgColor.r + ((pixel * (fgColor.r - bgColor.r)) / 255);
                uint8_t g = bgColor.g + ((pixel * (fgColor.g - bgColor.g)) / 255);
                uint8_t b = bgColor.b + ((pixel * (fgColor.b - bgColor.b)) / 255);

                // Create new color struct with lerp'd values
                Color finalColor = { r, g, b };

                // We need to do bounds checking before commiting the pixel write due to our transformations, or we
                // could write out-of-bounds of the frame buffer
                if (x < 0 || y < 0 || x >= this->width || y >= this->height)
                    continue;

                // If the pixel in the bitmap isn't blank, we'll draw it
                if(pixel != 0x00)
                    this->DrawPixel(x, y, finalColor);
            }
        }

        // Increment x offset for the next character
        xOffset += slot->advance.x >> 6;
    }
}
#endif

