#define _CRT_SECURE_NO_WARNINGS

#include <gtest/gtest.h>
#include <fmt/printf.h>

#include <Usagi/Runtime/File/RegularFile.hpp>

// #include <psapi.h>

using namespace usagi;

#define PAGE_SIZE 4096

/*
void print_mem(const PROCESS_MEMORY_COUNTERS_EX &mem)
{
    fmt::print("PeakWorkingSetSize: {}\n", mem.PeakWorkingSetSize);
    fmt::print("WorkingSetSize: {}\n", mem.WorkingSetSize);
    fmt::print("QuotaPeakPagedPoolUsage: {}\n", mem.QuotaPeakPagedPoolUsage);
    fmt::print("QuotaPagedPoolUsage: {}\n", mem.QuotaPagedPoolUsage);
    fmt::print("QuotaPeakNonPagedPoolUsage: {}\n", mem.QuotaPeakNonPagedPoolUsage);
    fmt::print("QuotaNonPagedPoolUsage: {}\n", mem.QuotaNonPagedPoolUsage);
    fmt::print("PagefileUsage: {}\n", mem.PagefileUsage);
    fmt::print("PeakPagefileUsage: {}\n", mem.PeakPagefileUsage);
    fmt::print("PrivateUsage: {}\n\n", mem.PrivateUsage);
}

auto get_mem_usage()
{
    PROCESS_MEMORY_COUNTERS_EX mem { };

    GetProcessMemoryInfo(
        GetCurrentProcess(),
        reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&mem),
        sizeof(PROCESS_MEMORY_COUNTERS_EX)
    );

    return mem;
}
*/

/*
void print_mem_info(const MEMORY_BASIC_INFORMATION &mem)
{
    fmt::print("BaseAddress: {}\n", mem.BaseAddress);
    fmt::print("AllocationBase: {}\n", mem.AllocationBase);
    fmt::print("AllocationProtect: {:#x}\n", mem.AllocationProtect);
    fmt::print("RegionSize: {}\n", mem.RegionSize);
    fmt::print("State: {:#x}\n", mem.State);
    fmt::print("Protect: {:#x}\n", mem.Protect);
    fmt::print("Type: {:#x}\n", mem.Type);
}

// https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualquery
auto virtual_query(void *base)
{
    MEMORY_BASIC_INFORMATION info;
    VirtualQuery(base, &info, sizeof(info));
    return info;
}

void seh_test_no_access_violation(MappedFileView &mapping, std::size_t pos)
{
    __try
    {
        // accessing the pages causes page faults but not access violation
        mapping.base_view_byte()[pos] = '1';
        // print_mem(get_mem_usage());
    }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
        EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH)
    {
        ASSERT_TRUE(false);
    }
}

void seh_test_access_violation(MappedFileView &mapping, std::size_t pos)
{
    __try
    {
        mapping.base_view_byte()[pos] = '1';
    }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
        EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH)
    {
        // ASSERT_TRUE(false);
        fmt::print("test");
    }
}
*/

auto temp_file()
{
    using namespace platform::file;

    std::unique_ptr file = std::make_unique<RegularFile>(
        (char8_t*)tmpnam(nullptr),
        FileOpenMode(OPEN_READ | OPEN_WRITE),
        FileOpenOptions(OPTION_ALWAYS_CREATE_NEW)
        );
    fmt::print("Temp file path: {}\n", (const char*)file->path().c_str());

    return std::move(file);
}

TEST(MemoryMappedFile, MappedAccess)
{
    auto file = temp_file();

    // the file is initially empty
    EXPECT_EQ(file->size(), 0);

    auto mapping = file->create_view(0, PAGE_SIZE * 2, 0);
    // test that creating file mapping extends the file length
    EXPECT_EQ(file->size(), PAGE_SIZE * 2);
    // print_mem_info(virtual_query(mapping.base_view()));

    // seh_test_no_access_violation(mapping, 0);
    // seh_test_no_access_violation(mapping, PAGE_SIZE);
    // seh_test_access_violation(mapping, PAGE_SIZE * 2);
    // print_mem_info(virtual_query(mapping.base_view()));

    mapping.offer(0, PAGE_SIZE);
    // print_mem_info(virtual_query(mapping.base_view()));

    // expect 0xc0000021 STATUS_ALREADY_COMMITTED
    // EXPECT_THROW(mapping.commit(0, PAGE_SIZE), NtException);
    // expect 0xC000001B STATUS_UNABLE_TO_DELETE_SECTION
    // EXPECT_THROW(mapping.decommit(0, PAGE_SIZE), NtException);
}

TEST(MemoryMappedFile, LoadState)
{
    using namespace platform::file;

    auto file = temp_file();
    // create a file with two pages
    auto path = file->path();
    {
        EXPECT_EQ(file->size(), 0);
        auto mapping = file->create_view(0, PAGE_SIZE * 2, 0);
        EXPECT_EQ(file->size(), PAGE_SIZE * 2);
        // write content at the beginning of each page
        mapping.base_view_byte()[0] = '1';
        mapping.base_view_byte()[PAGE_SIZE] = '2';
        // mapping closed by dtor
    }
    // close the file, too
    file.reset();
    // reopen the file
    file = std::make_unique<RegularFile>(
        path,
        // bug: cannot create read-only mappings
        FileOpenMode(OPEN_READ | OPEN_WRITE),
        FileOpenOptions()
    );
    {
        EXPECT_EQ(file->size(), PAGE_SIZE * 2);
        // creates a view of only one page
        auto mapping = file->create_view(PAGE_SIZE, PAGE_SIZE, 0);
        EXPECT_EQ(file->size(), PAGE_SIZE * 2);
        EXPECT_EQ(mapping.max_size(), PAGE_SIZE);
        // check content == the second page
        EXPECT_EQ(mapping.base_view_byte()[0], '2');
    }
}

TEST(MemoryMappedFile, ExtendLength)
{
    auto file = temp_file();

    // the file is initially empty
    EXPECT_EQ(file->size(), 0);

    auto mapping = file->create_view(PAGE_SIZE, PAGE_SIZE, 0);
    // test that creating file mapping extends the file length
    EXPECT_EQ(file->size(), PAGE_SIZE * 2);

    mapping.remap(PAGE_SIZE * 2);

    EXPECT_EQ(file->size(), PAGE_SIZE * 3);
    EXPECT_EQ(mapping.max_size(), PAGE_SIZE * 2);

    // seh_test_no_access_violation(mapping, PAGE_SIZE);
}
