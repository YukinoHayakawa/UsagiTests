#include <gtest/gtest.h>

#include <any>

#include <Usagi/Library/Memory/MemoryArena.hpp>
#include <Usagi/Modules/Common/Math/Matrix.hpp>
#include <Usagi/Modules/Runtime/Asset/details/AssetEnum.hpp>
#include <Usagi/Modules/Runtime/Asset/HeapAssetManager.hpp>
#include <Usagi/Modules/Runtime/Asset/details/AssetQuery.hpp>
#include <Usagi/Modules/Runtime/Asset/Package/AssetPackageFilesystem.hpp>
#include <Usagi/Modules/Runtime/Executive/ServiceAsyncWorker.hpp>

#include <Usagi/Modules/Runtime/HeapManager/HeapManager.hpp>
#include <Usagi/Modules/Runtime/HeapManager/details/ResourceConstructDelegate.hpp>
#include <Usagi/Runtime/Memory/View.hpp>

using namespace usagi;

class HeapAnyObject : public Heap
{
    std::map<std::uint64_t, std::any> mObjects;

public:
    // The heap is free to provide whatever kind of service to builders.
    // As long as the managed objects can be accessed via ids.
    std::any & allocate(const HeapResourceIdT id)
    {
        auto [it, inserted] = mObjects.try_emplace(id);
        assert(inserted);
        return it->second;
    }

    template <typename T>
    const T & resource(const HeapResourceIdT id)
    {
        const auto it = mObjects.find(id);
        assert(it != mObjects.end());
        return std::any_cast<T &>(it->second);
    }
};

class BasicStringBuilder
{
    std::string_view mString;

public:
    explicit BasicStringBuilder(std::string_view string)
        : mString(std::move(string))
    {
    }

    // std::uint64_t target_heap() const
    // {
    //     return 0;
    // }

    using TargetHeapT = HeapAnyObject;
    using ProductT = std::string;

    ResourceState construct(
        ResourceConstructDelegate<BasicStringBuilder> &delegate)
    {
        // todo via heap accessor
        // todo how to manage this shit? memory could not be properly managed.
        auto &str = delegate.allocate();
        str.emplace<std::string>(mString);
        return ResourceState::READY;
    }
};

TEST(HeapManagerTest, BasicObjectLoadingTest)
{
    HeapManager manager;
    StdTaskExecutor executor;

    manager.add_heap<HeapAnyObject>();

    auto accessor = manager.resource<BasicStringBuilder>(
        { },
        &executor,
        [] { return std::forward_as_tuple("hello."); }
    ).make_request();

    const auto str = accessor.await();

    EXPECT_EQ(*str, "hello.");
}

TEST(HeapManagerTest, SyncedObjectLoadingTest)
{
    HeapManager manager;

    manager.add_heap<HeapAnyObject>();

    auto accessor = manager.resource_transient<BasicStringBuilder>(
        "hello."
    );

    // lifetime extended
    const auto &str = accessor.await();

    EXPECT_EQ(*str, "hello.");
}

TEST(HeapManagerTest, ResourceCacheTest)
{
    HeapManager manager;
    StdTaskExecutor executor;

    manager.add_heap<HeapAnyObject>();

    auto accessor = manager.resource<BasicStringBuilder>(
        { },
        &executor,
        [] { return std::forward_as_tuple("hello."); }
    ).make_request();

    const auto &str = accessor.await();

    EXPECT_EQ(*str, "hello.");

    EXPECT_NE(accessor.descriptor(), HeapResourceDescriptor());

    EXPECT_EQ(manager.resource<BasicStringBuilder>(
        { },
        &executor,
        [] { return std::forward_as_tuple("hello."); }
    ).make_request().descriptor(), accessor.descriptor());
}

class StringConcatenationBuilder
{
    std::string_view mPrevString, mThisString;

public:
    StringConcatenationBuilder(
        std::string_view prev_string,
        std::string_view this_string)
        : mPrevString(std::move(prev_string))
        , mThisString(std::move(this_string))
    {
    }
    //
    // std::uint64_t target_heap() const
    // {
    //     return 0;
    // }

    using TargetHeapT = HeapAnyObject;
    using ProductT = std::string;

    ResourceState construct(
        ResourceConstructDelegate<StringConcatenationBuilder> &delegate)
    {
        auto prev_string = delegate.resource<BasicStringBuilder>(
            mPrevString
        ).await().cref();

        auto &this_str = delegate.allocate();
        this_str.emplace<std::string>(prev_string) += mThisString;
        return ResourceState::READY;
    }
};

TEST(HeapManagerTest, BuilderTypeValidation)
{
    HeapManager manager;
    StdTaskExecutor executor;

    manager.add_heap<HeapAnyObject>();

    auto accessor = manager.resource<BasicStringBuilder>(
        { },
        &executor,
        [] { return std::forward_as_tuple("hello."); }
    ).make_request();

    const auto &str = accessor.await();

    EXPECT_EQ(*str, "hello.");

    EXPECT_NE(accessor.descriptor(), HeapResourceDescriptor());

    EXPECT_THROW(manager.resource<StringConcatenationBuilder>(
        accessor.descriptor(),
        &executor,
        [] { return std::forward_as_tuple("hello.", "world."); }
    ).make_request(), std::runtime_error);
}

TEST(HeapManagerTest, ResourceSynthesisTest)
{
    HeapManager manager;
    StdTaskExecutor executor;

    manager.add_heap<HeapAnyObject>();

    auto accessor = manager.resource<StringConcatenationBuilder>(
        { },
        &executor,
        [] { return std::forward_as_tuple("hello.", "world."); }
    ).make_request();

    const auto &str = accessor.await();

    EXPECT_EQ(*str, "hello.world.");
}

class RawAssetMemoryViewBuilder
{
    std::string mAssetPath;

public:
    explicit RawAssetMemoryViewBuilder(std::string asset_path)
        : mAssetPath(std::move(asset_path))
    {
    }

    // std::uint64_t target_heap() const
    // {
    //     return 1;
    // }

    using TargetHeapT = HeapAssetManager;
    using ProductT = ReadonlyMemoryView;

    ResourceState construct(
        ResourceConstructDelegate<RawAssetMemoryViewBuilder> &delegate)
    {
        /*
         * Locate the asset package containing the requested asset following
         * overriding rule. See AssetPackageManager for details.
         */
        MemoryArena arena;
        // The query goes through asset manager to have proper synchronization.
        auto [status, query] = delegate.allocate(
            mAssetPath,
            arena
        );

        if(status == AssetStatus::MISSING)
            return ResourceState::FAILED_INVALID_DATA;
        if(status == AssetStatus::EXIST_BUSY)
            return ResourceState::FAILED_BUSY;
        if(status == AssetStatus::EXIST)
        {
            query->fetch();
            assert(query->ready());
            return ResourceState::READY;
        }

        USAGI_UNREACHABLE("Invalid asset status.");
    }
};

struct Pixel
{
    std::uint8_t r, g, b;

    Pixel() = default;

    Pixel(std::uint8_t r, std::uint8_t g, std::uint8_t b)
        : r(r)
        , g(g)
        , b(b)
    {
    }

    auto operator<=>(const Pixel &rhs) const = default;
};

struct Image
{
    std::vector<Pixel> pixels;
    int width, height;
};

//
// template <typename SrcHeap, typename SrcRes, typename DstHeap, typename DstRes>
// struct HeapTransfer;

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

template <typename SrcHeap, typename DstHeap>
struct HeapTransfer<SrcHeap, ReadonlyMemoryView, DstHeap, Image>
{
    ResourceState operator()(
        SrcHeap &src_heap,
        const ReadonlyMemoryView &src_res,
        DstHeap &dst_heap,
        Image &dst_res)
    {
        // todo should first determine the size of the allocation?
        int x, y, comp;
        const int suc = stbi_info_from_memory(
            static_cast<stbi_uc const *>(src_res.base_address()),
            (int)src_res.size(),
            &dst_res.width,
            &dst_res.height,
            &comp
        );
        assert(suc == 1);
        // todo: the resize should be delegated to some allocator and the memory should be properly managed.
        dst_res.pixels.resize(dst_res.width * dst_res.height, { });
        // todo manage memory
        const stbi_uc* data = stbi_load_from_memory(
            static_cast<stbi_uc const *>(src_res.base_address()),
            (int)src_res.size(),
            &x,
            &y,
            &comp,
            3
        );
        assert(data != nullptr);
        memcpy(dst_res.pixels.data(), data, x * y * sizeof(Pixel));

        return ResourceState::READY;
    }
};

// transfer function copies one object from one heap to another.
// it can involve data transformation.
class DecodedImageBuilder
{
    std::string mImagePath;

public:
    explicit DecodedImageBuilder(std::string image_path)
        : mImagePath(std::move(image_path))
    {
    }

    // std::uint64_t target_heap() const
    // {
    //     return 0;
    // }

    using TargetHeapT = HeapAnyObject;
    using ProductT = Image;

    ResourceState construct(
        ResourceConstructDelegate<DecodedImageBuilder> &delegate)
    {
        // allocate pixel buffer
        std::any &buf = delegate.allocate();
        auto &img = buf.emplace<Image>();

        auto suc = delegate.transfer<RawAssetMemoryViewBuilder>(
            img,  // dst resource
            mImagePath // builder params
        );

        assert(suc == ResourceState::READY);

        return ResourceState::READY;
    }
};

// image loading test?
TEST(HeapManagerTest, ResourceTransferTest)
{
    HeapManager manager;
    StdTaskExecutor executor;

    manager.add_heap<HeapAnyObject>();
    auto asset_mgr = manager.add_heap<HeapAssetManager>();
    asset_mgr->add_package(std::make_unique<AssetPackageFilesystem>("."));

    auto accessor = manager.resource<DecodedImageBuilder>(
        { },
        &executor,
        [] { return std::forward_as_tuple("test.png"); }
    ).make_request();

    const auto img = accessor.await();
    
    EXPECT_EQ(img->width, 8);
    EXPECT_EQ(img->height, 8);
    for(int i = 0; i < img->height; ++i)
    {
        for(int j = 0; j < img->width; ++j)
        {
            EXPECT_EQ(img->pixels[i * img->width + j], Pixel(91, 206, 250));
        }
    }
}
