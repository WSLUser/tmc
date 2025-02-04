#include "asset.h"

class TilesetAsset : public BaseAsset {
  public:
    using BaseAsset::BaseAsset;
    virtual void convertToHumanReadable(const std::vector<char>& baserom);
    virtual void buildToBinary();

  private:
    virtual std::filesystem::path generateAssetPath();
};