import { useState, useEffect } from 'react'
import { supabase } from './lib/supabase'
import { Trophy, Clock, Play, Square, Server, AlertCircle, Medal, Trash2 } from 'lucide-react'

function App() {
  const [matchHistory, setMatchHistory] = useState([])
  const [gameControl, setGameControl] = useState(null)
  const [isLoading, setIsLoading] = useState(true)
  const [error, setError] = useState(null)
  
  const p1Wins = matchHistory.filter(m => m.winner.includes('1')).length
  const p2Wins = matchHistory.filter(m => m.winner.includes('2')).length
  
  const avgReactionTime = matchHistory.length > 0
    ? Math.round(matchHistory.reduce((acc, curr) => acc + curr.reaction_time_ms, 0) / matchHistory.length)
    : 0

  const bestMatch = matchHistory.length > 0 
    ? matchHistory.reduce((min, curr) => curr.reaction_time_ms < min.reaction_time_ms ? curr : min, matchHistory[0])
    : null

  useEffect(() => {
    fetchInitialData()

    const historySubscription = supabase
      .channel('public:match_history')
      .on('postgres_changes', { event: 'INSERT', schema: 'public', table: 'match_history' }, (payload) => {
        setMatchHistory((current) => [payload.new, ...current])
      })
      .on('postgres_changes', { event: 'DELETE', schema: 'public', table: 'match_history' }, (payload) => {
        setMatchHistory((current) => current.filter(m => m.id !== payload.old.id))
      })
      .subscribe()

    const controlSubscription = supabase
      .channel('public:game_control')
      .on('postgres_changes', { event: 'UPDATE', schema: 'public', table: 'game_control' }, (payload) => {
        setGameControl(payload.new)
      })
      .subscribe()

    return () => {
      supabase.removeChannel(historySubscription)
      supabase.removeChannel(controlSubscription)
    }
  }, [])

  const fetchInitialData = async () => {
    try {
      setIsLoading(true)
      
      const { data: historyData, error: historyError } = await supabase
        .from('match_history')
        .select('*')
        .order('created_at', { ascending: false })
        .limit(50)
        
      if (historyError) throw historyError
      setMatchHistory(historyData)

      const { data: controlData, error: controlError } = await supabase
        .from('game_control')
        .select('*')
        .limit(1)
        .single()
        
      if (controlError && controlError.code !== 'PGRST116') {
         throw controlError
      }
      
      if (controlData) {
        setGameControl(controlData)
      }
    } catch (err) {
      console.error(err)
      setError("Unable to connect to the database.")
    } finally {
      setIsLoading(false)
    }
  }

  const handleGameAction = async () => {
    if (!gameControl?.id) return

    const isPlaying = gameControl.status === 'playing'
    const command = isPlaying ? 'stop' : 'start'
    const newStatus = isPlaying ? 'idle' : 'playing'

    try {
      setGameControl({ ...gameControl, command_pending: command, status: newStatus })

      const { error } = await supabase
        .from('game_control')
        .update({ command_pending: command, status: newStatus })
        .eq('id', gameControl.id)

      if (error) throw error
      
    } catch (err) {
      console.error(err)
      alert("Error sending command.")
      fetchInitialData()
    }
  }

  const handleResetStats = async () => {
    if (!window.confirm("Are you sure you want to reset all statistics and clear the history?")) return

    try {
      const { error } = await supabase
        .from('match_history')
        .delete()
        .neq('id', '00000000-0000-0000-0000-000000000000')

      if (error) throw error
      
      setMatchHistory([])
    } catch (err) {
      console.error(err)
      alert("Error resetting statistics.")
    }
  }

  const isPlaying = gameControl?.status === 'playing'
  const isCommandPending = gameControl?.command_pending !== null

  return (
    <div className="min-h-screen bg-slate-100 p-8">
      <div className="max-w-6xl mx-auto space-y-8">
        
        <header className="flex justify-between items-center bg-white p-6 rounded-2xl shadow-sm">
          <div>
            <h1 className="text-3xl font-bold text-slate-800">ReflexDuel IoT Dashboard</h1>
            <p className="text-slate-500 mt-1">Remote control and real-time statistics</p>
          </div>
          <div className="flex items-center space-x-2 text-emerald-600 bg-emerald-50 px-4 py-2 rounded-full font-medium">
            <Server className="w-5 h-5" />
            <span>System Connected</span>
          </div>
        </header>

        {error && (
          <div className="bg-red-50 border-l-4 border-red-500 p-4 rounded-md flex items-start space-x-3">
            <AlertCircle className="w-5 h-5 text-red-500 mt-0.5" />
            <p className="text-red-700">{error}</p>
          </div>
        )}

        <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
          
          <div className="lg:col-span-1 space-y-6">
            <div className="bg-white p-6 rounded-2xl shadow-sm">
              <h2 className="text-xl font-semibold mb-4 text-slate-800 border-b pb-2">Game Control</h2>
              
              <div className="flex flex-col items-center justify-center py-6 space-y-6">
                <button
                  onClick={handleGameAction}
                  disabled={isCommandPending}
                  className={`
                    flex flex-col items-center justify-center w-48 h-48 rounded-full shadow-lg transition-all duration-300
                    ${isCommandPending ? 'opacity-70 scale-95 cursor-wait' : 'hover:scale-105 active:scale-95'}
                    ${isPlaying ? 'bg-red-500 hover:bg-red-600' : 'bg-emerald-500 hover:bg-emerald-600'}
                  `}
                >
                  {isPlaying ? (
                    <>
                      <Square className={`w-16 h-16 text-white ${isCommandPending ? 'animate-pulse' : ''}`} fill="currentColor" />
                      <span className="mt-2 font-bold text-lg text-white">STOP MATCH</span>
                    </>
                  ) : (
                    <>
                      <Play className={`w-16 h-16 text-white ml-2 ${isCommandPending ? 'animate-pulse' : ''}`} fill="currentColor" />
                      <span className="mt-2 font-bold text-lg text-white">START MATCH</span>
                    </>
                  )}
                </button>
                
                <p className="text-sm text-center text-slate-500">
                  {isCommandPending
                    ? "Processing command..."
                    : isPlaying
                      ? "Match in progress... Click to stop"
                      : "Click to start a new match via UART"}
                </p>
              </div>
            </div>

            <div className="bg-gradient-to-br from-amber-100 to-orange-100 p-6 rounded-2xl shadow-sm border border-amber-200">
              <h2 className="text-xl font-semibold mb-4 text-amber-900 border-b border-amber-200 pb-2 flex items-center space-x-2">
                <Medal className="w-5 h-5" />
                <span>All-Time Record</span>
              </h2>
              
              {bestMatch ? (
                <div className="text-center py-4">
                  <div className="text-4xl font-extrabold text-amber-600 mb-2">{bestMatch.reaction_time_ms} ms</div>
                  <div className="text-amber-800 font-medium bg-amber-200/50 inline-block px-4 py-1 rounded-full">
                    Set by {bestMatch.winner}
                  </div>
                </div>
              ) : (
                <div className="text-center py-6 text-amber-700/70 italic text-sm">
                  No record set yet.
                </div>
              )}
            </div>

            <div className="bg-white p-6 rounded-2xl shadow-sm space-y-4">
              <div className="flex justify-between items-center border-b pb-2 mb-4">
                <h2 className="text-xl font-semibold text-slate-800">Global Statistics</h2>
                <button 
                  onClick={handleResetStats}
                  className="p-1.5 text-slate-400 hover:text-red-500 hover:bg-red-50 rounded-lg transition-colors"
                  title="Reset statistics"
                >
                  <Trash2 className="w-5 h-5" />
                </button>
              </div>
              
              <div className="flex justify-between items-center p-3 bg-blue-50 rounded-lg">
                <div className="flex items-center space-x-3">
                  <Trophy className="w-5 h-5 text-blue-500" />
                  <span className="font-medium text-slate-700">Player 1 Wins</span>
                </div>
                <span className="text-xl font-bold text-blue-700">{p1Wins}</span>
              </div>
              
              <div className="flex justify-between items-center p-3 bg-red-50 rounded-lg">
                <div className="flex items-center space-x-3">
                  <Trophy className="w-5 h-5 text-red-500" />
                  <span className="font-medium text-slate-700">Player 2 Wins</span>
                </div>
                <span className="text-xl font-bold text-red-700">{p2Wins}</span>
              </div>

              <div className="flex justify-between items-center p-3 bg-slate-50 rounded-lg">
                <div className="flex items-center space-x-3">
                  <Clock className="w-5 h-5 text-slate-500" />
                  <span className="font-medium text-slate-700">Average Time</span>
                </div>
                <span className="text-xl font-bold text-slate-700">{avgReactionTime} ms</span>
              </div>
            </div>
          </div>

          <div className="lg:col-span-2">
            <div className="bg-white p-6 rounded-2xl shadow-sm h-full">
              <h2 className="text-xl font-semibold mb-4 text-slate-800 border-b pb-2">Latest Matches</h2>
              
              {isLoading ? (
                <div className="flex justify-center items-center h-64">
                  <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-emerald-500"></div>
                </div>
              ) : matchHistory.length === 0 ? (
                <div className="text-center text-slate-500 py-12">
                  No matches recorded. Start a new match!
                </div>
              ) : (
                <div className="overflow-hidden rounded-xl border border-slate-200">
                  <table className="min-w-full divide-y divide-slate-200">
                    <thead className="bg-slate-50">
                      <tr>
                        <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-slate-500 uppercase tracking-wider">
                          Date and Time
                        </th>
                        <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-slate-500 uppercase tracking-wider">
                          Winner
                        </th>
                        <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-slate-500 uppercase tracking-wider">
                          Reaction Time
                        </th>
                      </tr>
                    </thead>
                    <tbody className="bg-white divide-y divide-slate-200">
                      {matchHistory.map((match) => (
                        <tr key={match.id} className="hover:bg-slate-50 transition-colors">
                          <td className="px-6 py-4 whitespace-nowrap text-sm text-slate-500">
                            {new Date(match.created_at).toLocaleString('en-US')}
                          </td>
                          <td className="px-6 py-4 whitespace-nowrap">
                            <span className={`px-3 py-1 inline-flex text-xs leading-5 font-semibold rounded-full 
                              ${match.winner.includes('1') ? 'bg-blue-100 text-blue-800' : 'bg-red-100 text-red-800'}`}>
                              {match.winner}
                            </span>
                          </td>
                          <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-slate-900">
                            {match.reaction_time_ms} ms
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              )}
            </div>
          </div>
          
        </div>
      </div>
    </div>
  )
}

export default App
