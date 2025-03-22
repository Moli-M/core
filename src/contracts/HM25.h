using namespace QPI;

struct HM252
{
};

struct HM25 : public ContractBase
{
public:
    struct Echo_input{};
    struct Echo_output{};

    struct Burn_input{};
    struct Burn_output{};

    struct SetToken_input
    {
        char name[20];
        char symbol[10];
        uint64_t totalSupply;
    };
    struct SetToken_output{};

    struct GetStats_input {};
    struct GetStats_output
    {
        uint64 numberOfEchoCalls;
        uint64 numberOfBurnCalls;
    };

private:
    uint64 numberOfEchoCalls;
    uint64 numberOfBurnCalls;

    struct Token {
        char name[20];
        char symbol[10];
        uint64_t totalSupply;
    };

    Token token;

    /**
    Send back the invocation amount
    */
    PUBLIC_PROCEDURE(Echo)
        state.numberOfEchoCalls++;
        if (qpi.invocationReward() > 0)
        {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
        }
    _

    /**
    * Burn all invocation amount
    */
    PUBLIC_PROCEDURE(Burn)
        state.numberOfBurnCalls++;
        if (qpi.invocationReward() > 0)
        {
            qpi.burn(qpi.invocationReward());
        }
    _

    PUBLIC_PROCEDURE(SetToken)        
        token.name = input.name;
        token.symbol = input.symbol;
        token.totalSupply = input.totalSupply;

        state.token = token;
    _

    PUBLIC_FUNCTION(GetStats)
        output.numberOfBurnCalls = state.numberOfBurnCalls;
        output.numberOfEchoCalls = state.numberOfEchoCalls;
    _

    REGISTER_USER_FUNCTIONS_AND_PROCEDURES

        REGISTER_USER_PROCEDURE(Echo, 1);
        REGISTER_USER_PROCEDURE(Burn, 2);
        REGISTER_USER_PROCEDURE(SetToken, 3);

        REGISTER_USER_FUNCTION(GetStats, 1);
    _

    INITIALIZE
        state.numberOfEchoCalls = 0;
        state.numberOfBurnCalls = 0;
    _
};
