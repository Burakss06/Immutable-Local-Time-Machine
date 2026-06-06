#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <utility>
#include "crypto.h"

// Dosyanın belirli bir zamandaki sürüm (versiyon) bilgisi
struct FileVersion {
    uint64_t timestamp;                  // Değişiklik zaman damgası (Epoch milisaniye)
    std::vector<Sha256Hash> blockHashes; // Dosyayı oluşturan blokların parmak izleri
};

class RadixTree {
public:
    struct Node {
        std::wstring edgeLabel;
        std::vector<FileVersion> versions;
        std::vector<std::unique_ptr<Node>> children;
        bool isFile = false;
        
        Node(std::wstring label = L"") : edgeLabel(std::move(label)) {}
    };

    RadixTree() : m_root(std::make_unique<Node>()) {}

    // Ağaca yeni bir dosya sürümü ekler
    void Insert(const std::wstring& path, const FileVersion& version) {
        InsertInternal(m_root.get(), path, version);
    }

    // Ağaçta dosya yolu arar ve sürüm geçmişini döner
    const std::vector<FileVersion>* Lookup(const std::wstring& path) const {
        return LookupInternal(m_root.get(), path);
    }

    // Ağacı dolaşır ve tüm dosya yollarını (reconstruct edilmiş haliyle) callback'e iletir
    void Traverse(const std::function<void(const std::wstring&, const std::vector<FileVersion>&)>& callback) const {
        for (const auto& child : m_root->children) {
            TraverseInternal(child.get(), L"", callback);
        }
    }

private:
    std::unique_ptr<Node> m_root;

    // İki dizge arasındaki en uzun ortak öneğin karakter sayısını döner
    static size_t GetCommonPrefixLength(const std::wstring& s1, const std::wstring& s2) {
        size_t len = 0;
        size_t minLen = (std::min)(s1.size(), s2.size());
        while (len < minLen && s1[len] == s2[len]) {
            len++;
        }
        return len;
    }

    // Önek sıkıştırmalı recursive ekleme algoritması
    void InsertInternal(Node* curr, const std::wstring& suffix, const FileVersion& version) {
        if (suffix.empty()) {
            curr->isFile = true;
            curr->versions.push_back(version);
            return;
        }

        for (auto& child : curr->children) {
            size_t commonLen = GetCommonPrefixLength(suffix, child->edgeLabel);
            if (commonLen > 0) {
                if (commonLen == child->edgeLabel.size()) {
                    // Düğüm etiketinin tamamı eşleşti, alt düğümde aramaya devam et
                    InsertInternal(child.get(), suffix.substr(commonLen), version);
                    return;
                } else {
                    // Kısmi eşleşme var, düğümü bölmemiz (split) gerekiyor!
                    std::wstring commonPrefix = child->edgeLabel.substr(0, commonLen);
                    std::wstring remainingLabel = child->edgeLabel.substr(commonLen);

                    // Eski düğümün tüm alt ağacını ve verisini taşıyan splitNode oluştur
                    auto splitNode = std::make_unique<Node>(remainingLabel);
                    splitNode->isFile = child->isFile;
                    splitNode->versions = std::move(child->versions);
                    splitNode->children = std::move(child->children);

                    // Mevcut alt düğümün etiketini ortak öneğe daralt ve splitNode'u altına bağla
                    child->edgeLabel = commonPrefix;
                    child->isFile = false;
                    child->versions.clear();
                    child->children.clear();
                    child->children.push_back(std::move(splitNode));

                    // Yeni eklenecek parçayı yerleştir
                    std::wstring remainingSuffix = suffix.substr(commonLen);
                    if (remainingSuffix.empty()) {
                        child->isFile = true;
                        child->versions.push_back(version);
                    } else {
                        auto newNode = std::make_unique<Node>(remainingSuffix);
                        newNode->isFile = true;
                        newNode->versions.push_back(version);
                        child->children.push_back(std::move(newNode));
                    }
                    return;
                }
            }
        }

        // Hiçbir ortak önek bulunamadı, yeni bir dal (child) oluştur
        auto newNode = std::make_unique<Node>(suffix);
        newNode->isFile = true;
        newNode->versions.push_back(version);
        curr->children.push_back(std::move(newNode));
    }

    // $O(K)$ hızında çalışan recursive arama algoritması
    const std::vector<FileVersion>* LookupInternal(const Node* curr, const std::wstring& suffix) const {
        if (suffix.empty()) {
            return curr->isFile ? &curr->versions : nullptr;
        }

        for (const auto& child : curr->children) {
            size_t commonLen = GetCommonPrefixLength(suffix, child->edgeLabel);
            if (commonLen == child->edgeLabel.size()) {
                // Etiket eşleşti, arama derinleşir
                return LookupInternal(child.get(), suffix.substr(commonLen));
            }
        }
        return nullptr;
    }

    // Rekürsif dolaşma algoritması (Yol parçalarını birleştirerek tam yolu çıkarır)
    void TraverseInternal(const Node* node, const std::wstring& currentPath, 
                          const std::function<void(const std::wstring&, const std::vector<FileVersion>&)>& callback) const {
        std::wstring newPath = currentPath + node->edgeLabel;
        if (node->isFile) {
            callback(newPath, node->versions);
        }
        for (const auto& child : node->children) {
            TraverseInternal(child.get(), newPath, callback);
        }
    }
};
