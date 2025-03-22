using namespace QPI;

struct QRC20Token : public ContractBase
{
public:
    struct Transfer_input
    {
        address to;
        uint64 amount;
    };
    struct Transfer_output {};

    struct BalanceOf_input
    {
        address account;
    };
    struct BalanceOf_output
    {
        uint64 balance;
    };

    struct GetMeta_input {};
    struct GetMeta_output
    {
        string name;
        string symbol;
        uint64 decimals;
        uint64 totalSupply;
    };

private:
    // State
    mapping<address, uint64> balances;
    string name = "QubicToken";
    string symbol = "QTK";
    uint64 decimals = 6;
    uint64 totalSupply = 1'000'000'000; // 1 million tokens, 6 decimals

    PUBLIC_PROCEDURE(Transfer)
        if (state.balances[qpi.invocator()] < input.amount)
        {
            qpi.panic("Insufficient balance");
        }

        state.balances[qpi.invocator()] -= input.amount;
        state.balances[input.to] += input.amount;
    _

    PUBLIC_FUNCTION(BalanceOf)
        output.balance = state.balances[input.account];
    _

    PUBLIC_FUNCTION(GetMeta)
        output.name = state.name;
        output.symbol = state.symbol;
        output.decimals = state.decimals;
        output.totalSupply = state.totalSupply;
    _

    REGISTER_USER_FUNCTIONS_AND_PROCEDURES

        REGISTER_USER_PROCEDURE(Transfer, 1);

        REGISTER_USER_FUNCTION(BalanceOf, 1);
        REGISTER_USER_FUNCTION(GetMeta, 2);
    _

    INITIALIZE
        // Assign total supply to creator
        state.balances[qpi.invocator()] = state.totalSupply;
    _
};
