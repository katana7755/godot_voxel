#ifndef VOXEL_STREAM_REGION_H
#define VOXEL_STREAM_REGION_H

#include "voxel_stream_file.h"
//#include <core/map.h>

class FileAccess;

// Loads and saves blocks to the filesystem, under a directory.
// Blocks are saved in region files to minimize I/O.
// It's a bit more complex but should deliver better performance.
// Loading and saving blocks in batches of similar regions makes a lot more sense here,
// because it allows to keep using the same file handles and avoid switching.
// Inspired by https://www.seedofandromeda.com/blogs/1-creating-a-region-file-system-for-a-voxel-game
//
class VoxelStreamRegion : public VoxelStreamFile {
	GDCLASS(VoxelStreamRegion, VoxelStreamFile)
public:
	VoxelStreamRegion();
	~VoxelStreamRegion();

	void emerge_blocks(Vector<BlockRequest> &p_blocks) override;
	void immerge_blocks(Vector<BlockRequest> &p_blocks) override;

	String get_directory() const;
	void set_directory(String dirpath);

protected:
	static void _bind_methods();

private:
	struct CachedRegion;
	struct RegionHeader;

	enum EmergeResult {
		EMERGE_OK,
		EMERGE_OK_FALLBACK,
		EMERGE_FAILED
	};

	EmergeResult _emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod);
	void _immerge_block(Ref<VoxelBuffer> voxel_buffer, Vector3i origin_in_voxels, int lod);

	Error save_meta();
	Error load_meta();
	Vector3i get_block_position_from_voxels(const Vector3i &origin_in_voxels) const;
	Vector3i get_region_position_from_blocks(const Vector3i &block_position) const;
	void clear_cache();
	String get_region_file_path(const Vector3i &region_pos, unsigned int lod) const;
	CachedRegion *open_region(const Vector3i region_pos, unsigned int lod, bool create_if_not_found);
	void close_region(CachedRegion *cache);
	int get_block_index_in_header(const Vector3i &rpos) const;
	Vector3i get_block_position_from_index(int i) const;
	int get_sector_count_from_bytes(int size_in_bytes) const;
	int get_region_header_size() const;
	CachedRegion *get_region_from_cache(const Vector3i pos, int lod) const;
	void remove_sectors_from_block(CachedRegion *p_region, Vector3i block_rpos, int p_sector_count);
	int get_sectors_count(const RegionHeader &header) const;
	void close_oldest_region();
	void save_header(CachedRegion *p_region);
	void pad_to_sector_size(FileAccess *f);

	// Orders block requests so those querying the same regions get grouped together
	struct BlockRequestComparator {
		VoxelStreamRegion *self = nullptr;

		// operator<
		_FORCE_INLINE_ bool operator()(const VoxelStreamRegion::BlockRequest &a, const VoxelStreamRegion::BlockRequest &b) const {
			if (a.lod < b.lod) {
				return true;
			} else if (a.lod > b.lod) {
				return false;
			}
			Vector3i bpos_a = self->get_block_position_from_voxels(a.origin_in_voxels);
			Vector3i bpos_b = self->get_block_position_from_voxels(b.origin_in_voxels);
			Vector3i rpos_a = self->get_region_position_from_blocks(bpos_a);
			Vector3i rpos_b = self->get_region_position_from_blocks(bpos_b);
			return rpos_a < rpos_b;
		}
	};

	struct Meta {
		uint8_t version = -1;
		uint8_t lod_count = 0;
		Vector3i block_size; // How many voxels in a block
		Vector3i region_size; // How many blocks in one region
		int sector_size = 0; // Blocks are stored at offsets multiple of that size
		// TODO World boundary
	};

	struct BlockInfo {
		uint32_t data = 0;

		inline uint32_t get_sector_index() const {
			return data >> 8;
		}

		inline void set_sector_index(uint32_t i) {
			CRASH_COND(i > 0xffffff);
			data = (i << 8) | (data & 0xff);
		}

		inline uint32_t get_sector_count() const {
			return data & 0xff;
		}

		inline void set_sector_count(uint32_t c) {
			CRASH_COND(c > 0xff);
			data = (c & 0xff) | (data & 0xffffff00);
		}
	};

	struct RegionHeader {
		uint8_t version;
		// Location and size of blocks, indexed by flat position.
		// This table always has the same size,
		// and the same index always corresponds to the same 3D position.
		std::vector<BlockInfo> blocks;
	};

	struct CachedRegion {
		Vector3i position;
		int lod = 0;
		bool file_exists = false;
		FileAccess *file_access = NULL;
		RegionHeader header;

		// List of sectors in the order they appear in the file,
		// and which position their block is. The same block can span multiple sectors.
		// This is essentially a reverse table of `header->blocks`.
		std::vector<Vector3i> sectors;

		uint64_t last_opened = 0;
		//uint64_t last_accessed;
	};

	String _directory_path;
	Meta _meta;
	bool _meta_loaded = false;
	bool _meta_saved = false;
	std::vector<CachedRegion *> _region_cache;
	int _max_open_regions = MIN(4, FOPEN_MAX);
};

#endif // VOXEL_STREAM_REGION_H
