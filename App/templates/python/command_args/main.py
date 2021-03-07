import sys

def main():
    try:    
        if len(sys.argv) >= 2:
            name = sys.argv[1]
        else:
            name = 'World'
        print 'Hello', name
        
        print 'Type something to display its type and value.'
        userInput = raw_input()
        print 'User input - type: %s, value: %s' % (type(userInput), userInput)
    except Exception as ex:
        print ex.message
        return 1 # indicates error, but not necessary
    else:
        return 0 # return 0 # indicates errorlessly exit, but not necessary    
    
# this is the standard boilerplate that calls the main() function
if __name__ == '__main__':
    # sys.exit(main(sys.argv)) # used to give a better look to exists
    main()
