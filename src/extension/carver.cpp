#include "carver.h"
#include <algorithm>

std::vector<CarvedFile> CarveData(const std::vector<uint8_t>& rawData, const FileSignature& sig) {
    std::vector<CarvedFile> results;
    if (rawData.size() < sig.header.size() + sig.footer.size()) return results;

    auto headerIt = rawData.begin();
    while (true) {
        // 1. Header (Başlangıç imzası) ara
        headerIt = std::search(headerIt, rawData.end(), sig.header.begin(), sig.header.end());
        if (headerIt == rawData.end()) break;

        // 2. Footer (Bitiş imzası) ara
        auto footerIt = std::search(headerIt + sig.header.size(), rawData.end(), sig.footer.begin(), sig.footer.end());
        if (footerIt == rawData.end()) {
            headerIt += sig.header.size(); // Footer yoksa ilerle
            continue;
        }

        // 3. Bulunan veriyi kopyala
        size_t fileSize = std::distance(headerIt, footerIt) + sig.footer.size();
        CarvedFile carved;
        carved.ext = sig.ext;
        carved.startOffset = std::distance(rawData.begin(), headerIt);
        carved.data.assign(headerIt, headerIt + fileSize);
        
        results.push_back(carved);
        headerIt = footerIt + sig.footer.size(); // Arama imlecini kaydır
    }
    return results;
}
