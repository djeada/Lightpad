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
        return 1 
    else:
        return 0 
    

if __name__ == '__main__':
    
    main()
