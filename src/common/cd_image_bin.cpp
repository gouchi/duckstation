#include "cd_image.h"
#include "cd_subchannel_replacement.h"
#include "file_system.h"
#include "log.h"
#include <cerrno>
Log_SetChannel(CDImageBin);

class CDImageBin : public CDImage
{
public:
  CDImageBin();
  ~CDImageBin() override;

  bool Open(const char* filename);

  bool ReadSubChannelQ(SubChannelQ* subq) override;

protected:
  bool ReadSectorFromIndex(void* buffer, const Index& index, LBA lba_in_index) override;

private:
  std::FILE* m_fp = nullptr;
  u64 m_file_position = 0;

  CDSubChannelReplacement m_sbi;
};

static std::string ReplaceExtension(std::string_view path, std::string_view new_extension)
{
  std::string_view::size_type pos = path.rfind('.');
  if (pos == std::string::npos)
    return std::string(path);

  std::string ret(path, 0, pos + 1);
  ret.append(new_extension);
  return ret;
}

CDImageBin::CDImageBin() = default;

CDImageBin::~CDImageBin()
{
  if (m_fp)
    std::fclose(m_fp);
}

bool CDImageBin::Open(const char* filename)
{
  m_filename = filename;
  m_fp = FileSystem::OpenCFile(filename, "rb");
  if (!m_fp)
  {
    Log_ErrorPrintf("Failed to open binfile '%s': errno %d", filename, errno);
    return false;
  }

  const u32 track_sector_size = RAW_SECTOR_SIZE;

  // determine the length from the file
  std::fseek(m_fp, 0, SEEK_END);
  const u32 file_size = static_cast<u32>(std::ftell(m_fp));
  std::fseek(m_fp, 0, SEEK_SET);

  m_lba_count = file_size / track_sector_size;

  SubChannelQ::Control control = {};
  TrackMode mode = TrackMode::Mode2Raw;
  control.data = mode != TrackMode::Audio;

  // Two seconds default pregap.
  const u32 pregap_frames = 2 * FRAMES_PER_SECOND;
  Index pregap_index = {};
  pregap_index.file_sector_size = track_sector_size;
  pregap_index.start_lba_on_disc = 0;
  pregap_index.start_lba_in_track = static_cast<LBA>(-static_cast<s32>(pregap_frames));
  pregap_index.length = pregap_frames;
  pregap_index.track_number = 1;
  pregap_index.index_number = 0;
  pregap_index.mode = mode;
  pregap_index.control.bits = control.bits;
  pregap_index.is_pregap = true;
  m_indices.push_back(pregap_index);

  // Data index.
  Index data_index = {};
  data_index.file_index = 0;
  data_index.file_offset = 0;
  data_index.file_sector_size = track_sector_size;
  data_index.start_lba_on_disc = pregap_index.length;
  data_index.track_number = 1;
  data_index.index_number = 1;
  data_index.start_lba_in_track = 0;
  data_index.length = m_lba_count;
  data_index.mode = mode;
  data_index.control.bits = control.bits;
  m_indices.push_back(data_index);

  // Assume a single track.
  m_tracks.push_back(
    Track{static_cast<u32>(1), data_index.start_lba_on_disc, static_cast<u32>(0), m_lba_count, mode, control});

  AddLeadOutIndex();

  m_sbi.LoadSBI(ReplaceExtension(filename, "sbi").c_str());

  return Seek(1, Position{0, 0, 0});
}

bool CDImageBin::ReadSubChannelQ(SubChannelQ* subq)
{
  if (m_sbi.GetReplacementSubChannelQ(m_position_on_disc, subq->data))
    return true;

  return CDImage::ReadSubChannelQ(subq);
}

bool CDImageBin::ReadSectorFromIndex(void* buffer, const Index& index, LBA lba_in_index)
{
  const u64 file_position = index.file_offset + (static_cast<u64>(lba_in_index) * index.file_sector_size);
  if (m_file_position != file_position)
  {
    if (std::fseek(m_fp, static_cast<long>(file_position), SEEK_SET) != 0)
      return false;

    m_file_position = file_position;
  }

  if (std::fread(buffer, index.file_sector_size, 1, m_fp) != 1)
  {
    std::fseek(m_fp, static_cast<long>(m_file_position), SEEK_SET);
    return false;
  }

  m_file_position += index.file_sector_size;
  return true;
}

std::unique_ptr<CDImage> CDImage::OpenBinImage(const char* filename)
{
  std::unique_ptr<CDImageBin> image = std::make_unique<CDImageBin>();
  if (!image->Open(filename))
    return {};

  return image;
}
