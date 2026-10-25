#include "gtest/gtest.h"
#include "source_io/module_hs/reference_exx_shape.h"

#include <array>
#include <map>
#include <memory>
#include <valarray>

namespace
{
struct Block
{
    std::vector<std::size_t> shape;
    std::shared_ptr<std::valarray<double>> data;
};
using Cell = std::pair<int,std::array<int,3>>;
using Reference = std::vector<std::map<int,std::map<Cell,Block>>>;
const std::vector<std::size_t> orbitals{2,1};

int rank()
{
    int value=0;
#ifdef __MPI
    MPI_Comm_rank(MPI_COMM_WORLD,&value);
#endif
    return value;
}
int size()
{
    int value=1;
#ifdef __MPI
    MPI_Comm_size(MPI_COMM_WORLD,&value);
#endif
    return value;
}
bool ready(const Reference* ref, const int spin, std::string& error)
{
    return ModuleIO::reference_exx_shapes_valid_on_all_ranks(ref,spin,orbitals,error
#ifdef __MPI
                                                           ,MPI_COMM_WORLD
#endif
    );
}
Block matrix()
{
    return Block{{2,1},std::make_shared<std::valarray<double>>(1.0,2)};
}
Cell pair()
{
    return Cell{1,{{0,0,0}}};
}
}

TEST(ReferenceExxShape, MissingPointerAndChannelVectorAreRejected)
{
    std::string error;
    EXPECT_FALSE(ready(nullptr,4,error));
    Reference absent;
    EXPECT_FALSE(ready(&absent,4,error));
    Reference wrong(2);
    EXPECT_FALSE(ready(&wrong,4,error));
}

TEST(ReferenceExxShape, EmptyLocalOwnersAndComputedZeroChannelsAreValid)
{
    for (const int spin : {1,2,4})
    {
        Reference ref(spin);
        if (rank()==0) ref[0][0][pair()]=matrix();
        std::string error;
        EXPECT_TRUE(ready(&ref,spin,error));
        EXPECT_TRUE(error.empty());
    }
}

TEST(ReferenceExxShape, OneRankMissingChannelsRejectsEveryRankCollectively)
{
    Reference ref(4);
    if (rank()==size()-1) ref.resize(0);
    std::string error;
    EXPECT_FALSE(ready(&ref,4,error));
    EXPECT_EQ(error.empty(),rank()!=size()-1);
}

TEST(ReferenceExxShape, TensorAtomShapeAndPayloadAreCheckedBeforeUse)
{
    for (int fault=0;fault<5;++fault)
    {
        Reference ref(4);
        if (rank()==size()-1)
        {
            Block block=matrix();
            int atom=0;
            if (fault==0) atom=-1;
            if (fault==1) block.shape={2,1,1};
            if (fault==2) block.shape={1,2};
            if (fault==3) block.data.reset();
            if (fault==4) block.data=std::make_shared<std::valarray<double>>(1.0,1);
            ref[0][atom][pair()]=block;
        }
        std::string error;
        EXPECT_FALSE(ready(&ref,4,error));
        EXPECT_EQ(error.empty(),rank()!=size()-1);
    }
}

int main(int argc,char** argv)
{
#ifdef __MPI
    MPI_Init(&argc,&argv);
#endif
    testing::InitGoogleTest(&argc,argv);
    const int result=RUN_ALL_TESTS();
#ifdef __MPI
    MPI_Finalize();
#endif
    return result;
}
