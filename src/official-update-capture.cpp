#include "bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QPointer>

#if defined(__arm__)
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

namespace {
// These are executable call sites, not offsets into a native C++ object.
// Both module setup routines allocate a shared UpdateEnabled and call the
// public QObject base constructor here, before assigning the derived vtable.
struct Target { const char *buildId; uintptr_t returnVA, gotVA; uint32_t call; };
constexpr Target targets[] = {
    {"6f4138dd626ea45d0f7f9d750537721a894d0562",0x212884,0x138a0a4,0xebf8e275}, // RM2 3.27.1.0
    {"2f0554fb42208c3862a0ae5015e377aae6ce87b4",0x300440,0x1619304,0xebf68d4e}, // RM2 3.29.1.0
    {"291f7caeccc371b3f74beecb8f4c620ef291a645",0x2e678c,0x17bd564,0xebf732f8} // RM2 3.30.0.109
};
using Constructor = void *(*)(QObject *, QObject *);
Constructor originalConstructor=nullptr;
uintptr_t nativeCaller=0;
bool installed=false;

__attribute__((noinline)) void *captureConstructor(QObject *object, QObject *parent) {
    const auto caller=reinterpret_cast<uintptr_t>(__builtin_return_address(0));
    void *result=originalConstructor(object,parent);
    if (caller==nativeCaller) {
        // QObject is initialized now, but its derived construction is not.
        // Inspect the public metaobject only after module setup has yielded.
        QPointer<QObject> guard(object);
        if (auto *app=QCoreApplication::instance())
            QMetaObject::invokeMethod(app,[guard] {
                if (guard && ManagerNavigation::shared()->bindOfficialUpdates(guard,nullptr))
                    qInfo("[extension-manager-ui] bound resident native UpdateEnabled");
            },Qt::QueuedConnection);
    }
    return result; // ARM constructor ABI returns this.
}

QByteArray buildId(const dl_phdr_info *info) {
    for (ElfW(Half) i=0;i<info->dlpi_phnum;++i) {
        const auto &p=info->dlpi_phdr[i];
        if (p.p_type!=PT_NOTE) continue;
        auto *cursor=reinterpret_cast<const unsigned char *>(info->dlpi_addr+p.p_vaddr);
        const auto *end=cursor+p.p_memsz;
        while (size_t(end-cursor)>=sizeof(ElfW(Nhdr))) {
            ElfW(Nhdr) note; std::memcpy(&note,cursor,sizeof(note));
            cursor+=sizeof(note);
            const size_t nameSize=(size_t(note.n_namesz)+3)&~size_t(3);
            const size_t descSize=(size_t(note.n_descsz)+3)&~size_t(3);
            if (nameSize>size_t(end-cursor) || descSize>size_t(end-cursor)-nameSize) break;
            if (note.n_type==NT_GNU_BUILD_ID && note.n_namesz==4 &&
                std::memcmp(cursor,"GNU",4)==0 && note.n_descsz==20)
                return QByteArray(reinterpret_cast<const char *>(cursor+nameSize),20).toHex();
            cursor+=nameSize+descSize;
        }
    }
    return {};
}

int install(dl_phdr_info *info,size_t,void *) {
    if (info->dlpi_name && *info->dlpi_name) return 0;
    const auto id=buildId(info);
    const Target *target=nullptr;
    for (const auto &candidate:targets) if (id==candidate.buildId) target=&candidate;
    if (!target) return 1;
    const uintptr_t caller=info->dlpi_addr+target->returnVA;
    const uintptr_t slotAddress=info->dlpi_addr+target->gotVA;
    int protection=0;
    bool executableCall=false;
    for (ElfW(Half) i=0;i<info->dlpi_phnum;++i) {
        const auto &p=info->dlpi_phdr[i];
        const uintptr_t start=info->dlpi_addr+p.p_vaddr,end=start+p.p_memsz;
        if (p.p_type==PT_LOAD && (p.p_flags&PF_X) && caller-4>=start && caller<=end)
            executableCall=true;
        if (p.p_type==PT_LOAD && slotAddress>=start && slotAddress+sizeof(void *)<=end) {
            if (p.p_flags&PF_R) protection|=PROT_READ;
            if (p.p_flags&PF_W) protection|=PROT_WRITE;
        }
    }
    if (!executableCall || !protection || *reinterpret_cast<const uint32_t *>(caller-4)!=target->call) return 1;
    for (ElfW(Half) i=0;i<info->dlpi_phnum;++i) {
        const auto &p=info->dlpi_phdr[i];
        const uintptr_t start=info->dlpi_addr+p.p_vaddr;
        if (p.p_type==PT_GNU_RELRO && slotAddress>=start && slotAddress<start+p.p_memsz)
            protection&=~PROT_WRITE;
    }
    auto *handle=dlopen("libQt6Core.so.6",RTLD_NOW|RTLD_NOLOAD);
    if (!handle) return 1;
    originalConstructor=reinterpret_cast<Constructor>(dlsym(handle,"_ZN7QObjectC2EPS_"));
    dlclose(handle); // QtCore remains loaded by xochitl and this extension.
    if (!originalConstructor) return 1;
    const long size=sysconf(_SC_PAGESIZE);
    if (size<=0) return 1;
    void *page=reinterpret_cast<void *>(slotAddress&~(uintptr_t(size)-1));
    if (!(protection&PROT_WRITE) && mprotect(page,size,protection|PROT_WRITE)!=0) return 1;
    nativeCaller=caller;
    __atomic_store_n(reinterpret_cast<void **>(slotAddress),reinterpret_cast<void *>(captureConstructor),__ATOMIC_RELEASE);
    installed=true;
    if (!(protection&PROT_WRITE) && mprotect(page,size,protection)!=0)
        qWarning("[extension-manager-ui] could not restore QObject GOT page protection");
    return 1;
}
}
#endif

void initializeOfficialUpdateCapture() {
#if defined(__arm__)
    dl_iterate_phdr(install,nullptr);
    if (!installed) qWarning("[extension-manager-ui] resident UpdateEnabled capture unavailable for this executable");
#endif
}
